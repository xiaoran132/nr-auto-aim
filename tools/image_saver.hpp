#pragma once

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

#include <opencv2/opencv.hpp>

namespace tools
{
// 生成形如 20261106_221034_584.jpg 的时间戳文件名
inline std::string timestamp_name()
{
  using namespace std::chrono;

  const auto now = system_clock::now();
  const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
  const std::time_t t = system_clock::to_time_t(now);

  std::tm tm{};
  localtime_r(&t, &tm);

  std::ostringstream ss;
  ss << std::put_time(&tm, "%Y%m%d_%H%M%S") << '_' << std::setw(3) << std::setfill('0')
     << ms.count() << ".jpg";
  return ss.str();
}

// 保存到 dir，目录不存在时自动创建
inline bool save_image(const cv::Mat & frame, const std::string & dir, std::string & out_path)
{
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec && !std::filesystem::exists(dir)) {
    std::fprintf(stderr, "[FAIL] 无法创建目录 %s: %s\n", dir.c_str(), ec.message().c_str());
    return false;
  }

  out_path = (std::filesystem::path(dir) / timestamp_name()).string();
  return cv::imwrite(out_path, frame);
}
}  // namespace tools
