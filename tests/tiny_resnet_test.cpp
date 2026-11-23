#include <cmath>
#include <iostream>
#include <string>

#include <opencv2/dnn.hpp>
#include <opencv2/imgcodecs.hpp>

int main(int argc, char ** argv)
{
  constexpr int input_size = 32;
  constexpr int output_size = 9;
  const std::string model_path = PROJECT_ROOT_DIR "/assets/tiny_resnet.onnx";

  if (argc > 2 || (argc == 2 && std::string(argv[1]) == "--help")) {
    std::cout << "用法: " << argv[0] << " [灰度测试图片路径]\n"
              << "省略图片路径时使用合成灰图进行推理测试。\n";
    return argc > 2 ? 2 : 0;
  }

  try {
    cv::Mat image(input_size, input_size, CV_8UC1, cv::Scalar(128));
    if (argc == 2) {
      image = cv::imread(argv[1], cv::IMREAD_GRAYSCALE);
      if (image.empty()) {
        std::cerr << "无法读取测试图片: " << argv[1] << '\n';
        return 1;
      }
    }

    cv::dnn::Net net = cv::dnn::readNetFromONNX(model_path);
    // 测试用预处理：缩放到 32×32、归一化到 [0, 1]，生成 [1, 1, 32, 32] 的张量。
    const cv::Mat blob = cv::dnn::blobFromImage(image, 1.0 / 255.0, {input_size, input_size});
    net.setInput(blob);
    const cv::Mat output = net.forward();
    if (output.dims != 2 || output.rows != 1 || output.cols != output_size ||
        output.type() != CV_32FC1) {
      std::cerr << "模型输出应为 float32 [1, 9]。\n";
      return 1;
    }

    const float * scores = output.ptr<float>(0);
    int best_index = 0;
    for (int i = 0; i < output_size; ++i) {
      if (!std::isfinite(scores[i])) {
        std::cerr << "模型输出包含非有限值，索引: " << i << '\n';
        return 1;
      }
      if (scores[i] > scores[best_index]) best_index = i;
    }

    std::cout << "输入: " << (argc == 2 ? argv[1] : "合成灰图") << '\n';
    std::cout << "输出形状: [" << output.rows << ", " << output.cols << "]\n";
    for (int i = 0; i < output_size; ++i) {
      std::cout << "score[" << i << "] = " << scores[i] << '\n';
    }
    std::cout << "最高分索引（从 0 开始）: " << best_index << '\n';
  } catch (const cv::Exception & e) {
    std::cerr << "tiny_resnet 推理测试失败: " << e.what() << '\n';
    return 1;
  }

  return 0;
}
