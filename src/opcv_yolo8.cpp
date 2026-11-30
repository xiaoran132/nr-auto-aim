#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <vector>
#include <string>
#include <cmath>

using namespace std;

// =====================================================================
// 模型输出格式（已从 yolov8.xml / yolov8.bin 逆向确认）：
//   forward() 输出 shape = [1, 14, 3549]
//     14 通道按顺序（channel-major）：
//       [0:4]   bbox  [cx, cy, w, h]  —— 已乘 stride，416x416 像素坐标
//       [4:6]   2 个类别分数          —— 已 sigmoid（模型内嵌了 Sigmoid）
//       [6:14]  4 个关键点 (x,y)      —— 已解码，416x416 像素坐标
//   3549 = 52*52 + 26*26 + 13*13（anchor-free，输入 416，每格一个候选）
//   与 yolov5 的区别：输入 416 而非 640；无 objectness；类别已 sigmoid。
// =====================================================================

const int   INPUT_SIZE     = 416;   // 模型输入边长（注意：不是 640）
const int   NUM_KEYPOINTS  = 4;     // 关键点个数
const int   NUM_CLASSES    = 2;     // 类别数
const int   OUTPUT_DIM     = 4 + NUM_CLASSES + NUM_KEYPOINTS * 2;  // = 14

const float CONF_THRESHOLD = 0.25f; // 置信度阈值：类别分数（已 sigmoid）低于它丢弃
const float NMS_THRESHOLD  = 0.45f; // 非极大值抑制 IoU 阈值

// 类别名：请替换成你训练时用的 2 个真实类名
static const vector<string> CLASS_NAMES = {
    "class0", "class1"
};

// 一个检测结果
struct Detection {
    float                   score;
    int                     class_id;
    cv::Rect                bbox;        // 模型输出的 bbox（反算回原图坐标）
    vector<cv::Point2f>     keypoints;   // 4 个关键点（已反算回原图坐标）
};

static inline float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

// letterbox 预处理：等比缩放 + 灰色填充，返回处理后的 416x416 图；
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

// 后处理：把 [1, 14, 3549] 解析成检测结果列表（含置信度过滤）
static vector<Detection> postprocess(const cv::Mat& output,
                                     float scale, int pad_x, int pad_y,
                                     int img_w, int img_h) {
    // yolov8 输出是 [1, 14, 3549]（通道优先），先把 batch 维压掉得到 [14, 3549]，
    // 再转置成 [3549, 14]，这样一行就是一个候选，方便按行读取。
    cv::Mat out = output.reshape(1, output.size[1]); // [14, 3549]
    cv::transpose(out, out);                         // [3549, 14]

    vector<Detection> dets;

    for (int i = 0; i < out.rows; ++i) {
        const float* row = out.ptr<float>(i);

        // bbox [cx, cy, w, h]，已是像素坐标
        float cx = row[0], cy = row[1], w = row[2], h = row[3];

        // 类别分数已 sigmoid，取最大作为置信度（YOLOv8 无 objectness）
        int   best_cls = 0;
        float best_score = row[4];
        for (int c = 1; c < NUM_CLASSES; ++c) {
            if (row[4 + c] > best_score) {
                best_score = row[4 + c];
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

        // 4 个关键点：通道 [6:14] = (x1,y1, x2,y2, x3,y3, x4,y4)，反算回原图
        for (int k = 0; k < NUM_KEYPOINTS; ++k) {
            float x = (row[6 + k * 2 + 0] - pad_x) / scale;
            float y = (row[6 + k * 2 + 1] - pad_y) / scale;
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
        cv::rectangle(img, d.bbox, cv::Scalar(0, 255, 0), 2);

        // 4 个关键点：画圆点，并按顺序连线
        for (size_t k = 0; k < d.keypoints.size(); ++k) {
            cv::circle(img, d.keypoints[k], 3, cv::Scalar(0, 0, 255), -1);
            if (k > 0)
                cv::line(img, d.keypoints[k - 1], d.keypoints[k], cv::Scalar(255, 0, 0), 1);
        }

        string label = CLASS_NAMES[d.class_id] + " " + to_string(d.score).substr(0, 4);
        cv::putText(img, label, cv::Point(d.bbox.x, std::max(d.bbox.y - 5, 0)),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 255), 2);
    }
}

int main(int argc, char** argv) {
    // 读取模型文件路径（OpenVINO IR 格式）
    //根据开发需要自行修改
    string xml_path = PROJECT_ROOT_DIR "/assents_sp/yolov8.xml";
    string bin_path = PROJECT_ROOT_DIR "/assents_sp/yolov8.bin";

    // 1. 加载 YOLO 模型（OpenVINO IR 格式）
    cv::dnn::Net net;
    try {
        net = cv::dnn::readNetFromModelOptimizer(xml_path, bin_path);
    }
    catch (const cv::Exception& e) {
        cerr << "Error loading model: " << e.what() << endl;//判断是否加载成功
        return -1;
    }

    // 2. 打开摄像头
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        cerr << "Error_opening camera (index 0)" << endl;//判断是否打开摄像头
        return -1;
    }

    cv::Mat frame;// 用于存放摄像头画面
    bool printed_shape = false;// 用于只打印一次输出 shape

    while (true) {
        cap >> frame;
        if (frame.empty()) {
            cout << "读取摄像头画面失败" << endl;
            break;
        }

        // 3. letterbox 预处理（等比缩放 + 填充，而不是直接拉伸变形）
        float scale = 0.0f;
        int pad_x = 0, pad_y = 0;
        cv::Mat input = letterbox(frame, scale, pad_x, pad_y);

        // 4. 转 blob 并推理
        cv::Mat blob = cv::dnn::blobFromImage(input, 1.0 / 255.0,
                                              cv::Size(INPUT_SIZE, INPUT_SIZE),
                                              cv::Scalar(), true, false); // swapRB=true: BGR->RGB
        net.setInput(blob);
        cv::Mat output = net.forward();

        // 打印一次输出 shape，方便核对（应为 1 x 14 x 3549）
        if (!printed_shape) {
            cout << "output dims: " << output.dims << "  [";
            for (int i = 0; i < output.dims; ++i)
                cout << output.size[i] << (i + 1 < output.dims ? ", " : "");
            cout << "]" << endl;
            printed_shape = true;
        }

        // 5. 后处理：解析 -> 过滤 -> NMS
        vector<Detection> dets = postprocess(output, scale, pad_x, pad_y,
                                             frame.cols, frame.rows);
        nms(dets, NMS_THRESHOLD);

        // 6. 画框并显示
        draw(frame, dets);
        cv::imshow("yolo8_Out", frame);

        if (cv::waitKey(1) == 27) break; // ESC 退出
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}
