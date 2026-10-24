// 03_yolo_camera.cpp —— 最简单的 OpenCV 调用 YOLO + 摄像头实时识别
//
// 原理：OpenCV 的 cv::dnn 模块可以直接加载 ONNX 格式的 YOLO 模型做推理，
//       不需要 OpenVINO / TensorRT 等额外库，最适合入门。
//
// 模型：YOLOv5s（ultralytics 官方导出的 ONNX，opset 较低，OpenCV DNN 100% 兼容）。
//       YOLOv8 / YOLO11 官方导出的 ONNX 用了高版本 opset，OpenCV DNN 解析会报
//       "Concat ... Inconsistent shape" 错误，所以这里用 YOLOv5。
//
// 运行：编译后在可执行文件目录下放好 yolov5s.onnx 和 coco.names，直接运行即可。


//       也可以命令行指定： 03_yolo_camera.exe 模型路径.onnx 类别文件.names
//       按 q 退出。

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// 检测参数
const float CONF_THRESHOLD = 0.5f;   // 置信度阈值：低于这个分数的框丢弃
const float NMS_THRESHOLD  = 0.4f;   // 非极大值抑制阈值：去掉重叠的重复框
const int   INPUT_SIZE     = 640;    // YOLO 要求的输入边长（正方形）

// 从 coco.names 读取 80 个类别名，每行一个
std::vector<std::string> load_classes(const std::string & path)
{
  std::vector<std::string> classes;
  std::ifstream file(path);
  if (!file.is_open()) {
    std::cerr << "无法打开类别文件: " << path << std::endl;
    return classes;
  }
  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty()) classes.push_back(line);
  }
  return classes;
}

int main(int argc, char ** argv)
{
  // 模型和类别文件路径（可用命令行参数覆盖）
  const std::string model_path = (argc > 1) ? argv[1] : "yolov5s.onnx";
  const std::string names_path = (argc > 2) ? argv[2] : "coco.names";

  auto classes = load_classes(names_path);
  if (classes.empty()) return -1;

  // 1. 加载 YOLO 模型（ONNX 格式）
  cv::dnn::Net net = cv::dnn::readNetFromONNX(model_path);

  // 2. 打开默认摄像头
  cv::VideoCapture cap(0);
  if (!cap.isOpened()) {
    std::cerr << "无法打开摄像头（index 0）" << std::endl;
    return -1;
  }

  cv::Mat frame;
  while (true) {
    cap >> frame;                 // 读取一帧
    if (frame.empty()) break;

    // 3. letterbox 预处理：把画面等比缩放到 640x640，四周用灰色填充
    //    （YOLO 需要正方形输入，直接拉伸会变形，所以要等比缩放 + 填充）
    float scale = std::min(
      static_cast<float>(INPUT_SIZE) / frame.cols,
      static_cast<float>(INPUT_SIZE) / frame.rows);
    int new_w = static_cast<int>(frame.cols * scale);
    int new_h = static_cast<int>(frame.rows * scale);
    int pad_x = (INPUT_SIZE - new_w) / 2;
    int pad_y = (INPUT_SIZE - new_h) / 2;

    cv::Mat resized;
    cv::resize(frame, resized, {new_w, new_h});
    cv::Mat input(INPUT_SIZE, INPUT_SIZE, CV_8UC3, cv::Scalar(114, 114, 114));
    resized.copyTo(input(cv::Rect(pad_x, pad_y, new_w, new_h)));

    // 转成 blob（归一化到 [0,1]）并送入网络
    cv::Mat blob = cv::dnn::blobFromImage(input, 1.0 / 255.0, {INPUT_SIZE, INPUT_SIZE}, cv::Scalar(), true, false);
    net.setInput(blob);

    // 4. 前向推理。YOLOv5 输出形状 [1, 25200, 85]
    //    25200 = 候选框数量
    //    85 = 4(中心x,y + 宽w,高h) + 1(物体置信度 objectness) + 80(COCO 类别分数)
    cv::Mat output = net.forward();
    int num_anchors = output.size[1];     // 25200
    int num_channels = output.size[2];    // 85
    int num_classes = num_channels - 5;   // 80（YOLOv5 比 v8 多 1 个 objectness 通道）
    const float * data = reinterpret_cast<const float *>(output.data);

    // 5. 后处理：解析每个候选框，过滤低置信度，收集框
    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    std::vector<int> class_ids;

    for (int i = 0; i < num_anchors; ++i) {
      const float * row = data + i * num_channels;   // 指向第 i 个候选框

      // 物体置信度：这个候选框里"有物体"的概率
      float objectness = row[4];
      if (objectness < CONF_THRESHOLD) continue;

      // 找到这个候选框分数最高的类别（类别分数在 row[5..84]）
      int best_class = -1;
      float best_score = 0.0f;
      for (int c = 0; c < num_classes; ++c) {
        float score = row[5 + c];
        if (score > best_score) {
          best_score = score;
          best_class = c;
        }
      }

      // 最终置信度 = 物体置信度 × 类别分数
      float confidence = objectness * best_score;
      if (confidence < CONF_THRESHOLD) continue;

      // 中心点 + 宽高（letterbox 坐标系）
      float cx = row[0];
      float cy = row[1];
      float w  = row[2];
      float h  = row[3];

      // 从 letterbox 坐标还原回原始画面坐标
      float x1 = (cx - w / 2 - pad_x) / scale;
      float y1 = (cy - h / 2 - pad_y) / scale;
      float x2 = (cx + w / 2 - pad_x) / scale;
      float y2 = (cy + h / 2 - pad_y) / scale;

      boxes.push_back(cv::Rect(cvRound(x1), cvRound(y1), cvRound(x2 - x1), cvRound(y2 - y1)));
      scores.push_back(confidence);
      class_ids.push_back(best_class);
    }

    // 6. NMS：去掉重叠的重复框
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, scores, CONF_THRESHOLD, NMS_THRESHOLD, indices);

    // 7. 绘制结果
    for (int idx : indices) {
      cv::Rect box = boxes[idx];
      std::string label = classes[class_ids[idx]] + " " +
        std::to_string(static_cast<int>(scores[idx] * 100)) + "%";
      cv::rectangle(frame, box, cv::Scalar(0, 255, 0), 2);
      cv::putText(frame, label, cv::Point(box.x, box.y - 5),
        cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
    }

    cv::imshow("YOLO 实时检测", frame);
    if (cv::waitKey(1) == 'q') break;   // 按 q 退出
  }

  return 0;
}
