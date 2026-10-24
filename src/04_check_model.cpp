// 04_check_model.cpp —— 检查 ONNX 模型的输出形状（不接摄像头）
//
// 用途：加载一个 YOLO ONNX 模型，喂一张 640x640 的图，打印输出张量的形状。
//       写后处理代码之前，先看这里的输出形状，就知道该怎么解析了。
//
// 运行： 04_check_model.exe [模型路径.onnx]
//       （默认读取同目录下的 yolov5s.onnx）

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

#include <iostream>
#include <string>

int main(int argc, char ** argv)
{
  const std::string model_path = (argc > 1) ? argv[1] : "yolov5s.onnx";

  // 加载模型
  cv::dnn::Net net = cv::dnn::readNetFromONNX(model_path);
  std::cout << "模型加载成功: " << model_path << std::endl;

  // 造一张 640x640 的灰图喂进去（模型只在 forward 时才真正解析/执行所有层）
  cv::Mat input(640, 640, CV_8UC3, cv::Scalar(114, 114, 114));
  cv::Mat blob = cv::dnn::blobFromImage(input, 1.0 / 255.0, {640, 640},
                                        cv::Scalar(), true, false);
  net.setInput(blob);
  cv::Mat output = net.forward();

  // 打印输出张量的维度数 + 每个维度的大小
  std::cout << "输出维度数: " << output.dims << std::endl;
  for (int i = 0; i < output.dims; ++i)
    std::cout << "  size[" << i << "] = " << output.size[i] << std::endl;

  // 顺便打印元素总数和数据类型，帮助确认解析方式
  std::cout << "元素总数: " << output.total() << std::endl;
  std::cout << "数据类型: " << cv::typeToString(output.type()) << std::endl;

  return 0;
}
