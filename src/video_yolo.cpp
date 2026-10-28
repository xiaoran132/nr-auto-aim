// =====================================================================
// 模型输出格式（已从 yolo11.xml / yolo11.bin 逆向确认）：
//   forward() 输出 shape = [1, 50, 8400]
//     50 通道按顺序（channel-major）：
//       [0:4]   bbox  [cx, cy, w, h]  —— 已乘 stride，640x640 像素坐标
//       [4:42]  38 个类别分数        —— 已 sigmoid（模型内嵌了 Sigmoid）
//       [42:50] 4 个关键点 (x,y)     —— 已解码，640x640 像素坐标
//   8400 = 80*80 + 40*40 + 20*20（anchor-free，输入 640，每格一个候选）
//
// 注意：本文件用 OpenVINO C++ API 直接推理，而不是 OpenCV dnn 的
// readNetFromModelOptimizer。原因是 YOLO11 模型带 C2PSA 注意力块 +
// Detect 头的动态 shape 算子（ShapeOf/Mod/Greater/Gather），会让
// OpenCV dnn 的 OpenVINO 后端在加载阶段栈溢出崩溃（0xC0000409），
// 而 OpenVINO 2024.6 运行时本身能正常加载并推理该模型。
// =====================================================================

#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>          // 只用 blobFromImage 做预处理，不碰 OpenVINO 后端
#include <openvino/openvino.hpp>    // OpenVINO C++ API
#include <vector>
#include <string>
#include <cmath>
#include <cstring>
#include<iomanip>

using namespace std;

const int   INPUT_SIZE     = 640;   // 模型输入边长
const int   NUM_KEYPOINTS  = 4;     // 关键点个数
const int   NUM_CLASSES    = 38;    // 类别数
const int   CLS_OFFSET     = 4;                       // 类别起始通道
const int   KPT_OFFSET     = 4 + NUM_CLASSES;         // 关键点起始通道
const int   OUTPUT_DIM     = 4 + NUM_CLASSES + NUM_KEYPOINTS * 2;  // = 50
const int   NUM_CANDIDATES = 8400;   // 候选数 = 80*80 + 40*40 + 20*20

const float CONF_THRESHOLD = 0.25f; // 置信度阈值：类别分数（已 sigmoid）低于它丢弃
const float NMS_THRESHOLD  = 0.45f; // 非极大值抑制 IoU 阈值

double fps = 0.0;

double T_start = (double)cv::getTickCount();//获取时间戳

// 类别名：请替换成你训练时用的 38 个真实类名（当前是占位符）
//运用循环开始直接生成38个。写入动态数组
static vector<string> make_class_names() {
    vector<string> v;
    for (int i = 0; i < NUM_CLASSES; ++i)
        v.push_back("class" + to_string(i));
    return v;
}
static const vector<string> CLASS_NAMES = make_class_names();//

// 一个检测结果
struct Detection {
    float                   score;
    int                     class_id;
    cv::Rect                bbox;        // 模型输出的 bbox（反算回原图坐标）
    vector<cv::Point2f>     keypoints;   // 4 个关键点（已反算回原图坐标）
};

// letterbox 预处理：等比缩放 + 灰色填充，返回处理后的 640x640 图；
// 同时回填 scale / pad_x / pad_y，后处理反算坐标要用。
static cv::Mat letterbox(const cv::Mat& src, float& scale, int& pad_x, int& pad_y) {
    scale = std::min(static_cast<float>(INPUT_SIZE) / src.cols,
                     static_cast<float>(INPUT_SIZE) / src.rows);
    int new_w = static_cast<int>(std::round(src.cols * scale));
    int new_h = static_cast<int>(std::round(src.rows * scale));

    cv::Mat resized;
    cv::resize(src, resized, cv::Size(new_w, new_h));

    pad_x = (INPUT_SIZE - new_w) / 2;
    pad_y = (INPUT_SIZE - new_h) / 2;

    cv::Mat canvas(INPUT_SIZE, INPUT_SIZE, CV_8UC3, cv::Scalar(114, 114, 114));
    resized.copyTo(canvas(cv::Rect(pad_x, pad_y, new_w, new_h)));
    return canvas;
}

// 后处理：把 [1, 50, 8400]（channel-major 的 float*）解析成检测结果列表
static vector<Detection> postprocess(const float* out,
                                     float scale, int pad_x, int pad_y,
                                     int img_w, int img_h) {
    vector<Detection> dets;

    for (int i = 0; i < NUM_CANDIDATES; ++i) {
        // bbox [cx, cy, w, h]，已是像素坐标
        float cx = out[0 * NUM_CANDIDATES + i];
        float cy = out[1 * NUM_CANDIDATES + i];
        float w  = out[2 * NUM_CANDIDATES + i];
        float h  = out[3 * NUM_CANDIDATES + i];

        // 类别分数已 sigmoid，取最大作为置信度（无 objectness）
        int   best_cls = 0;
        float best_score = out[CLS_OFFSET * NUM_CANDIDATES + i];
        for (int c = 1; c < NUM_CLASSES; ++c) {
            float s = out[(CLS_OFFSET + c) * NUM_CANDIDATES + i];
            if (s > best_score) {
                best_score = s;
                best_cls = c;
            }
        }
        if (best_score < CONF_THRESHOLD)
            continue;

        Detection d;
        d.score    = best_score;
        d.class_id = best_cls;

        // bbox 反算回原图：先减填充，再除以缩放系数
        float x1 = (cx - w / 2.0f - pad_x) / scale;
        float y1 = (cy - h / 2.0f - pad_y) / scale;
        float x2 = (cx + w / 2.0f - pad_x) / scale;
        float y2 = (cy + h / 2.0f - pad_y) / scale;
        x1 = std::max(0.0f, std::min(static_cast<float>(img_w - 1), x1));
        y1 = std::max(0.0f, std::min(static_cast<float>(img_h - 1), y1));
        x2 = std::max(0.0f, std::min(static_cast<float>(img_w - 1), x2));
        y2 = std::max(0.0f, std::min(static_cast<float>(img_h - 1), y2));
        d.bbox = cv::Rect(static_cast<int>(x1), static_cast<int>(y1),
                          static_cast<int>(x2 - x1 + 1),
                          static_cast<int>(y2 - y1 + 1));

        // 4 个关键点：通道 [KPT_OFFSET : KPT_OFFSET+8]，反算回原图
        for (int k = 0; k < NUM_KEYPOINTS; ++k) {
            float x = (out[(KPT_OFFSET + k * 2 + 0) * NUM_CANDIDATES + i] - pad_x) / scale;
            float y = (out[(KPT_OFFSET + k * 2 + 1) * NUM_CANDIDATES + i] - pad_y) / scale;
            x = std::max(0.0f, std::min(static_cast<float>(img_w - 1), x));
            y = std::max(0.0f, std::min(static_cast<float>(img_h - 1), y));
            d.keypoints.push_back(cv::Point2f(x, y));
        }

        dets.push_back(d);
    }
    return dets;
}

// 简单贪心 NMS，按 bbox 的 IoU 去重
static void nms(vector<Detection>& dets, float iou_thresh) {
    // 先按置信度从高到低排
    std::sort(dets.begin(), dets.end(),
              [](const Detection& a, const Detection& b) { return a.score > b.score; });
    vector<Detection> kept;
    for (auto& d : dets) {
        bool keep = true;
        for (auto& k : kept) {
            cv::Rect inter = d.bbox & k.bbox;
            float inter_area = static_cast<float>(inter.area());
            if (inter_area <= 0.0f) continue;
            float iou = inter_area / static_cast<float>(d.bbox.area() + k.bbox.area() - inter.area());
            if (iou > iou_thresh) { keep = false; break; }
        }
        if (keep) kept.push_back(d);
    }
    dets = std::move(kept);
}

// 在画面上画框、关键点和标签
static void draw(cv::Mat& img, const vector<Detection>& dets) {
    for (const auto& d : dets) {
        cv::rectangle(img, d.bbox, cv::Scalar(0, 255, 0), 2);//d.bbox	矩形区域（左上角 + 宽高，或左上+右下
        // 4 个关键点：画圆点，并按顺序连线
        for (size_t k = 0; k < d.keypoints.size(); ++k) {
            cv::circle(img, d.keypoints[k], 3, cv::Scalar(0, 0, 255), -1);//画出关键原点
            //d.keypoints[k]	圆心坐标
            //3	圆的半径 = 3 像素
            //cv::Scalar(0, 0, 255)	BGR 顺序 → 红色
            //-1	线宽为 -1 表示实心圆
            if (k > 0)
                cv::line(img, d.keypoints[k - 1], d.keypoints[k], cv::Scalar(255, 0, 0), 1);//相邻关键点连线
        }

        string label = CLASS_NAMES[d.class_id] + " " + to_string(d.score).substr(0, 4);//标记类型
        if(CLASS_NAMES[d.class_id] == "class9") cout<<"text"<<endl;//类型在这个测试代码当中用的是CLASS_NAMES[d.class_id]进行标记
        cv::putText(img, label, cv::Point(d.bbox.x, std::max(d.bbox.y - 5, 0)),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 0, 255), 1);
                    //(0, 255, 255)表示颜色（黄），(0, 0, 255)表示颜色（红）
                    //1表示线条粗细 0.4表示字体大小，
                    //FONT_HERSHEY_SIMPLEX,表示字体（OpenCV 内置的无衬线字体）
    }
}

int main(int argc, char** argv) {
    // 读取模型文件路径（OpenVINO IR 格式）
    //根据开发需要自行修改
    string xml_path = R"(assents_sp/yolo11.xml)";
    string bin_path = R"(assents_sp/yolo11.bin)";

    // 1. 加载 YOLO 模型并编译（OpenVINO C++ API）
    ov::Core            core;
    ov::CompiledModel   compiled;
    ov::InferRequest    infer_request;
    try {
        std::shared_ptr<ov::Model> model = core.read_model(xml_path, bin_path);
        compiled = core.compile_model(model, "CPU");
        infer_request = compiled.create_infer_request();
    }
    catch (const std::exception& e) {
        cerr << "Error loading model: " << e.what() << endl;//判断是否加载成功
        return -1;
    }

    // 2. 打开视频文件进行测试（不读取摄像头）
    cv::VideoCapture cap(R"(video_test/test.mp4)");
    //cv::VideoCapture cap(0);   //打开摄像头

    if (!cap.isOpened()) {
        cerr << "Error_opening video file (test.mp4)" << endl;//判断是否打开视频
        return -1;
    }

    cv::Mat frame;// 用于存放视频画面
    bool printed_shape = false;// 用于只打印一次输出 shape

    while (true) {
        cap >> frame;
        if (frame.empty()) {
            cout << "视频读取结束或读取失败" << endl;
            break;
        }

        double T_end = (double)cv::getTickCount();
        double time = (T_end - T_start)/cv::getTickFrequency();//获取时间差

        if(time > 0){
            fps = 0.9 * fps + 0.1 * (1.0 / time);
        }
        T_start = T_end;

        stringstream str_fps;
            str_fps  <<"FPS:"<<fixed<<setprecision(1)<<fps;


        cv::putText(frame,
            str_fps.str(),
            cv::Point(10,30),
            cv::FONT_HERSHEY_SIMPLEX,
            1.0,
            cv::Scalar(0,0,225),
            1,
            cv::LINE_AA);

        // 3. letterbox 预处理（等比缩放 + 填充，而不是直接拉伸变形）
        float scale = 0.0f;
        int pad_x = 0, pad_y = 0;
        cv::Mat input = letterbox(frame, scale, pad_x, pad_y);

        // 4. 转 blob（[1,3,640,640] float32 NCHW，BGR->RGB，除以 255），填进 OpenVINO 输入张量
        cv::Mat blob = cv::dnn::blobFromImage(input, 1.0 / 255.0,
                                              cv::Size(INPUT_SIZE, INPUT_SIZE),
                                              cv::Scalar(), true, false); // swapRB=true: BGR->RGB
        ov::Tensor in_tensor = infer_request.get_input_tensor(0);
        std::memcpy(in_tensor.data<float>(), blob.ptr<float>(),
                    blob.total() * blob.elemSize());

        // 5. 推理
        infer_request.infer();
        const ov::Tensor& out_tensor = infer_request.get_output_tensor(0);
        const float* out = out_tensor.data<float>();

        // 打印一次输出 shape，方便核对（应为 1 x 50 x 8400）
        if (!printed_shape) {
            ov::Shape sh = out_tensor.get_shape();
            cout << "output dims: [";
            for (size_t i = 0; i < sh.size(); ++i)
                cout << sh[i] << (i + 1 < sh.size() ? ", " : "");
            cout << "]" << endl;
            printed_shape = true;
        }

        // 6. 后处理：解析 -> 过滤 -> NMS
        vector<Detection> dets = postprocess(out, scale, pad_x, pad_y,
                                             frame.cols, frame.rows);
        nms(dets, NMS_THRESHOLD);

        // 7. 画框并显示
        draw(frame, dets);
        cv::imshow("yolo11_Out", frame);

        if (cv::waitKey(1) == 27) break; // ESC 退出
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}
