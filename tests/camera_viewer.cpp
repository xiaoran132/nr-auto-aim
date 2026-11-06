// =====================================================================
// 相机取流窗口 + 抓拍 + 实时调参
//   画面来源：configs/camera.yaml 选择的相机（usbcamera / hikrobot）
//
//   画面窗口：
//     s / 空格  = 抓拍保存到 ./img/tmp/
//     d         = 打印当前参数（可直接复制到 configs/camera.yaml）
//     q / ESC   = 退出
//
//   参数窗口 camera_controls：
//     拖动滑条即可实时修改曝光、亮度、对比度等，观察画面效果；
//     调好后按 d 导出对应 YAML。
//
// 用法：
//   ./camera_viewer              # 打开窗口实时显示
//   ./camera_viewer --snapshot   # 抓取一帧保存后退出（无需窗口）
// =====================================================================

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

#include "io/camera.hpp"
#include "tools/image_saver.hpp"

namespace
{
struct Control
{
  std::string name;
  int prop;
  int offset;  // 例如 hue 的 -180 偏移

  Control(std::string n, int p, int off) : name(std::move(n)), prop(p), offset(off) {}
};

io::Camera * g_camera = nullptr;
bool g_ready = false;

void on_trackbar(int value, void * userdata)
{
  if (!g_ready || !g_camera) return;
  auto * control = static_cast<Control *>(userdata);
  const int target = value + control->offset;
  if (!g_camera->setProperty(control->prop, target))
    std::fprintf(
      stderr, "[WARN] %s=%d 设置失败（相机不支持或当前模式不允许）\n", control->name.c_str(),
      target);
}

int prop_int(const io::Camera & cam, int prop)
{
  const double v = cam.getProperty(prop);
  return (v < 0.0) ? -1 : static_cast<int>(std::lround(v));
}

std::string prop_text(const io::Camera & cam, int prop)
{
  const int v = prop_int(cam, prop);
  return (v < 0) ? std::string("-") : std::to_string(v);
}

void dump_params(const io::Camera & cam)
{
  const int fcc = prop_int(cam, cv::CAP_PROP_FOURCC);
  char fourcc[5] = {
    static_cast<char>(fcc & 0xff), static_cast<char>((fcc >> 8) & 0xff),
    static_cast<char>((fcc >> 16) & 0xff), static_cast<char>((fcc >> 24) & 0xff), 0};

  const int auto_exp = prop_int(cam, cv::CAP_PROP_AUTO_EXPOSURE);

  std::cout << "\n# ===== 当前相机参数（复制到 configs/camera.yaml）=====\n";
  if (fcc > 0) std::cout << "fourcc: \"" << fourcc << "\"\n";
  std::cout << "image_width: " << prop_int(cam, cv::CAP_PROP_FRAME_WIDTH) << "\n";
  std::cout << "image_height: " << prop_int(cam, cv::CAP_PROP_FRAME_HEIGHT) << "\n";
  std::cout << "fps: " << prop_int(cam, cv::CAP_PROP_FPS) << "\n";
  std::cout << "auto_exposure: " << (auto_exp >= 1 ? 1 : 0) << "\n";
  std::cout << "exposure: " << prop_int(cam, cv::CAP_PROP_EXPOSURE) << "\n";
  std::cout << "brightness: " << prop_int(cam, cv::CAP_PROP_BRIGHTNESS) << "\n";
  std::cout << "contrast: " << prop_int(cam, cv::CAP_PROP_CONTRAST) << "\n";
  std::cout << "saturation: " << prop_int(cam, cv::CAP_PROP_SATURATION) << "\n";
  std::cout << "hue: " << prop_int(cam, cv::CAP_PROP_HUE) << "\n";
  std::cout << "sharpness: " << prop_int(cam, cv::CAP_PROP_SHARPNESS) << "\n";
  std::cout << "gamma: " << prop_int(cam, cv::CAP_PROP_GAMMA) << "\n";
  std::cout << "auto_wb: " << prop_int(cam, cv::CAP_PROP_AUTO_WB) << "\n";
  std::cout << "# =====================================================\n" << std::flush;
}

void setup_controls(io::Camera & cam, std::vector<std::unique_ptr<Control>> & controls)
{
  cv::namedWindow("camera_controls", cv::WINDOW_NORMAL);

  auto add = [&](const char * name, int prop, int max, int offset = 0) {
    const double raw = cam.getProperty(prop);
    if (raw < 0.0) return;  // 该相机不支持这个参数

    int pos = static_cast<int>(std::lround(raw)) - offset;
    pos = std::clamp(pos, 0, max);

    auto control = std::make_unique<Control>(name, prop, offset);
    cv::createTrackbar(name, "camera_controls", nullptr, max, on_trackbar, control.get());
    cv::setTrackbarPos(name, "camera_controls", pos);
    controls.push_back(std::move(control));
  };

  add("auto_exp", cv::CAP_PROP_AUTO_EXPOSURE, 1);
  add("exposure", cv::CAP_PROP_EXPOSURE, 10000);
  add("brightness", cv::CAP_PROP_BRIGHTNESS, 255);
  add("contrast", cv::CAP_PROP_CONTRAST, 255);
  add("saturation", cv::CAP_PROP_SATURATION, 255);
  add("hue", cv::CAP_PROP_HUE, 360, -180);
  add("sharpness", cv::CAP_PROP_SHARPNESS, 255);
  add("gamma", cv::CAP_PROP_GAMMA, 500);
  add("auto_wb", cv::CAP_PROP_AUTO_WB, 1);

  cv::resizeWindow("camera_controls", 420, 400);
  cv::moveWindow("camera_controls", 720, 50);
}
}  // namespace

int main(int argc, char ** argv)
{
  bool snapshot = false;
  bool dump_only = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--snapshot") snapshot = true;
    else if (arg == "--dump") dump_only = true;
    else if (arg == "-h" || arg == "--help") {
      std::cout << "用法: " << argv[0] << " [--snapshot] [--dump]\n"
                << "  --snapshot  抓取一帧保存后退出\n"
                << "  --dump      打印当前参数后退出\n"
                << "  窗口模式: s/空格 保存, d 导出参数, q/ESC 退出\n";
      return 0;
    }
  }

  const std::string save_dir = PROJECT_ROOT_DIR "/img/tmp";

  io::Camera cam(PROJECT_ROOT_DIR "/configs/camera.yaml");
  if (!cam.isOpened()) {
    std::cerr << "[FAIL] 打开相机失败（检查 configs/camera.yaml）" << std::endl;
    return -1;
  }

  if (dump_only) {
    dump_params(cam);
    cam.close();
    return 0;
  }

  if (snapshot) {
    cv::Mat frame;
    for (int i = 0; i < 10; ++i) {
      if (!cam.read(frame)) break;
    }

    if (frame.empty() || !cam.read(frame)) {
      std::cerr << "[FAIL] 取流失败" << std::endl;
      cam.close();
      return -1;
    }

    std::string path;
    if (!tools::save_image(frame, save_dir, path)) {
      std::cerr << "[FAIL] 图片保存失败" << std::endl;
      cam.close();
      return -1;
    }
    std::cout << "[OK] 已保存: " << path << std::endl;
    cam.close();
    return 0;
  }

  cv::namedWindow("camera_viewer", cv::WINDOW_NORMAL);
  cv::moveWindow("camera_viewer", 50, 50);

  std::vector<std::unique_ptr<Control>> controls;
  controls.reserve(16);
  g_camera = &cam;
  setup_controls(cam, controls);
  g_ready = true;

  std::cout << "[OK] 画面窗口: s/空格 抓拍, d 导出参数, q/ESC 退出\n"
            << "[OK] 参数窗口 camera_controls: 拖动滑条实时调参" << std::endl;

  cv::Mat frame;
  double fps = 0.0;
  auto t_prev = std::chrono::steady_clock::now();

  while (true) {
    if (!cam.read(frame) || frame.empty()) {
      std::cerr << "[FAIL] 取流失败" << std::endl;
      break;
    }

    const auto t_now = std::chrono::steady_clock::now();
    const double dt = std::chrono::duration<double>(t_now - t_prev).count();
    t_prev = t_now;
    if (dt > 0.0) fps = 0.9 * fps + 0.1 * (1.0 / dt);

    cv::Mat display = frame.clone();

    const int auto_exp = prop_int(cam, cv::CAP_PROP_AUTO_EXPOSURE);
    std::ostringstream info;
    info << "Exp " << prop_text(cam, cv::CAP_PROP_EXPOSURE) << "  Bri "
         << prop_text(cam, cv::CAP_PROP_BRIGHTNESS) << "  Con "
         << prop_text(cam, cv::CAP_PROP_CONTRAST) << "  Sat "
         << prop_text(cam, cv::CAP_PROP_SATURATION) << "  Hue "
         << prop_text(cam, cv::CAP_PROP_HUE) << "  AE "
         << (auto_exp >= 1 ? "auto" : "man");
    cv::putText(
      display, info.str(), cv::Point(10, 25), cv::FONT_HERSHEY_SIMPLEX, 0.5,
      cv::Scalar(0, 255, 0), 1, cv::LINE_AA);

    std::ostringstream hint;
    hint << "FPS " << std::fixed << std::setprecision(1) << fps
         << "   [s] save   [d] dump yaml   [q/ESC] quit";
    cv::putText(
      display, hint.str(), cv::Point(10, 50), cv::FONT_HERSHEY_SIMPLEX, 0.5,
      cv::Scalar(0, 255, 0), 1, cv::LINE_AA);

    cv::imshow("camera_viewer", display);

    const int key = cv::waitKey(1) & 0xff;
    if (key == 27 || key == 'q') break;

    if (key == 's' || key == ' ') {
      std::string path;
      if (tools::save_image(frame, save_dir, path))
        std::cout << "[OK] 已保存: " << path << std::endl;
      else
        std::cerr << "[FAIL] 图片保存失败" << std::endl;
    }

    if (key == 'd') dump_params(cam);
  }

  g_ready = false;
  g_camera = nullptr;
  cam.close();
  cv::destroyAllWindows();
  std::cout << "[OK] 相机已关闭" << std::endl;
  return 0;
}
