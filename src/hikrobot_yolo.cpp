// =====================================================================
// 海康工业相机(HIKROBOT MV-CU013-AOUC) + YOLO11 自动瞄准推理
//   视频源: HikRobotCamera (MVS SDK 取流), 替代原来的 test.mp4
//   推理: 与 video_yolo.cpp 相同 (OpenVINO 直接加载 yolo11.xml/bin)
//
// 模型输出格式: forward() 输出 [1, 50, 8400]
//   [0:4]   bbox [cx,cy,w,h]; [4:42] 38 类分数(已 sigmoid);
//   [42:50] 4 个关键点 (x,y)。8400 = 80*80+40*40+20*20。
// =====================================================================

#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>          // 只用 blobFromImage 做预处理
#include <openvino/openvino.hpp>    // OpenVINO C++ API
#include <vector>
#include <string>
#include <cmath>
#include <cstring>
#include <iomanip>

#include "hikrobot_camera.hpp"

using namespace std;

const int   INPUT_SIZE     = 640;   // 模型输入边长
const int   NUM_KEYPOINTS  = 4;     // 关键点个数
const int   NUM_CLASSES    = 38;    // 类别数
const int   CLS_OFFSET     = 4;                       // 类别起始通道
const int   KPT_OFFSET     = 4 + NUM_CLASSES;         // 关键点起始通道
const int   NUM_CANDIDATES = 8400;   // 候选数 = 80*80 + 40*40 + 20*20

const float CONF_THRESHOLD = 0.25f; // 置信度阈值
const float NMS_THRESHOLD  = 0.45f; // NMS IoU 阈值

double fps = 0.0;
double T_start = (double)cv::getTickCount();

// 类别名(占位, 换成你训练时的真实类名)
static vector<string> make_class_names() {
    vector<string> v;
    for (int i = 0; i < NUM_CLASSES; ++i)
        v.push_back("class" + to_string(i));
    return v;
}
static const vector<string> CLASS_NAMES = make_class_names();

struct Detection {
    float                   score;
    int                     class_id;
    cv::Rect                bbox;
    vector<cv::Point2f>     keypoints;
};

// letterbox 预处理: 等比缩放 + 灰色填充到 640x640
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

// 后处理: [1,50,8400] channel-major float* -> 检测结果列表
static vector<Detection> postprocess(const float* out,
                                     float scale, int pad_x, int pad_y,
                                     int img_w, int img_h) {
    vector<Detection> dets;
    for (int i = 0; i < NUM_CANDIDATES; ++i) {
        float cx = out[0 * NUM_CANDIDATES + i];
        float cy = out[1 * NUM_CANDIDATES + i];
        float w  = out[2 * NUM_CANDIDATES + i];
        float h  = out[3 * NUM_CANDIDATES + i];

        int   best_cls = 0;
        float best_score = out[CLS_OFFSET * NUM_CANDIDATES + i];
        for (int c = 1; c < NUM_CLASSES; ++c) {
            float s = out[(CLS_OFFSET + c) * NUM_CANDIDATES + i];
            if (s > best_score) { best_score = s; best_cls = c; }
        }
        if (best_score < CONF_THRESHOLD) continue;

        Detection d;
        d.score    = best_score;
        d.class_id = best_cls;

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

// 贪心 NMS
static void nms(vector<Detection>& dets, float iou_thresh) {
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

// 画框、关键点、标签
static void draw(cv::Mat& img, const vector<Detection>& dets) {
    for (const auto& d : dets) {
        cv::rectangle(img, d.bbox, cv::Scalar(0, 255, 0), 2);
        for (size_t k = 0; k < d.keypoints.size(); ++k) {
            cv::circle(img, d.keypoints[k], 3, cv::Scalar(0, 0, 255), -1);
            if (k > 0)
                cv::line(img, d.keypoints[k - 1], d.keypoints[k], cv::Scalar(255, 0, 0), 1);
        }
        string label = CLASS_NAMES[d.class_id] + " " + to_string(d.score).substr(0, 4);
        cv::putText(img, label, cv::Point(d.bbox.x, std::max(d.bbox.y - 5, 0)),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 0, 255), 1);
    }
}

int main(int argc, char** argv) {
    // 相机参数: 曝光 2ms / 增益 16 (可命令行覆盖, 与 sp_vision_25 配置一致)
    double exposure_ms = (argc > 1) ? atof(argv[1]) : 2.0;
    double gain        = (argc > 2) ? atof(argv[2]) : 16.0;

    // 1. 加载 YOLO 模型 (OpenVINO)
    string xml_path = PROJECT_ROOT_DIR "/assents_sp/yolo11.xml";
    string bin_path = PROJECT_ROOT_DIR "/assents_sp/yolo11.bin";

    ov::Core            core;
    ov::CompiledModel   compiled;
    ov::InferRequest    infer_request;
    try {
        std::shared_ptr<ov::Model> model = core.read_model(xml_path, bin_path);
        compiled = core.compile_model(model, "CPU");
        infer_request = compiled.create_infer_request();
    } catch (const std::exception& e) {
        cerr << "Error loading model: " << e.what() << endl;
        return -1;
    }

    // 2. 打开海康相机
    HikRobotCamera cam;
    if (!cam.open(exposure_ms, gain)) {
        cerr << "打开海康相机失败!" << endl;
        return -1;
    }
    cout << "海康相机已打开 (exposure=" << exposure_ms << "ms, gain=" << gain << ")" << endl;

    cv::Mat frame;
    bool printed_shape = false;

    while (true) {
        if (!cam.read(frame)) {
            cerr << "取流失败" << endl;
            break;
        }

        double T_end = (double)cv::getTickCount();
        double time = (T_end - T_start) / cv::getTickFrequency();
        if (time > 0) fps = 0.9 * fps + 0.1 * (1.0 / time);
        T_start = T_end;

        stringstream str_fps;
        str_fps << "FPS:" << fixed << setprecision(1) << fps;
        cv::putText(frame, str_fps.str(), cv::Point(10, 30),
                    cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 225), 1, cv::LINE_AA);

        // 3. letterbox 预处理
        float scale = 0.0f;
        int pad_x = 0, pad_y = 0;
        cv::Mat input = letterbox(frame, scale, pad_x, pad_y);

        // 4. blob -> OpenVINO 输入张量
        cv::Mat blob = cv::dnn::blobFromImage(input, 1.0 / 255.0,
                                              cv::Size(INPUT_SIZE, INPUT_SIZE),
                                              cv::Scalar(), true, false);
        ov::Tensor in_tensor = infer_request.get_input_tensor(0);
        std::memcpy(in_tensor.data<float>(), blob.ptr<float>(),
                    blob.total() * blob.elemSize());

        // 5. 推理
        infer_request.infer();
        const ov::Tensor& out_tensor = infer_request.get_output_tensor(0);
        const float* out = out_tensor.data<float>();

        if (!printed_shape) {
            ov::Shape sh = out_tensor.get_shape();
            cout << "output dims: [";
            for (size_t i = 0; i < sh.size(); ++i)
                cout << sh[i] << (i + 1 < sh.size() ? ", " : "");
            cout << "]" << endl;
            printed_shape = true;
        }

        // 6. 后处理 + NMS
        vector<Detection> dets = postprocess(out, scale, pad_x, pad_y,
                                             frame.cols, frame.rows);
        nms(dets, NMS_THRESHOLD);

        // 7. 画框显示
        draw(frame, dets);
        cv::imshow("hikrobot_yolo", frame);

        if (cv::waitKey(1) == 27) break;  // ESC 退出
    }

    cam.close();
    cv::destroyAllWindows();
    return 0;
}
