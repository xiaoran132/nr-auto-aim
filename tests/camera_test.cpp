// =====================================================================
// 相机取流最小测试：按 configs/camera.yaml 的 camera_name 打开相机，
// 连续抓 N 帧 -> 统计帧率 -> 保存一张图。
//   usbcamera —— 普通 USB / UVC 摄像头（cv::VideoCapture）
//   hikrobot  —— 海康 USB3 Vision 工业相机（MVS SDK）
// 用法: ./camera_test [抓取帧数]
// =====================================================================

#include <iostream>
#include <opencv2/opencv.hpp>
#include <string>

#include "io/camera.hpp"

using namespace std;

int main(int argc, char** argv) {
    int frames = (argc > 1) ? atoi(argv[1]) : 30;

    io::Camera cam(PROJECT_ROOT_DIR "/configs/camera.yaml");
    if (!cam.isOpened()) {
        cerr << "[FAIL] 打开相机失败（检查 configs/camera.yaml）" << endl;
        return -1;
    }
    cout << "[OK] 相机已打开, 开始抓 " << frames << " 帧..." << endl;

    cv::Mat img;
    int64 t0 = cv::getTickCount();
    int ok = 0;
    for (int i = 0; i < frames; ++i) {
        if (!cam.read(img)) {
            cerr << "[FAIL] 第 " << i << " 帧取流失败" << endl;
            break;
        }
        ++ok;
        if (i == 0) {
            cout << "[OK] 首帧尺寸: " << img.cols << "x" << img.rows
                 << " 通道: " << img.channels() << endl;
        }
    }
    double sec = (cv::getTickCount() - t0) / cv::getTickFrequency();
    cout << "[OK] 成功抓取 " << ok << "/" << frames << " 帧, 用时 " << sec
         << "s, 平均帧率 " << (sec > 0 ? ok / sec : 0) << " fps" << endl;

    if (ok > 0 && !img.empty()) {
        string out_path = PROJECT_ROOT_DIR "/camera_test.jpg";
        cv::imwrite(out_path, img);
        cout << "[OK] 已保存首帧到 " << out_path << endl;
    }

    cam.close();
    cout << "[OK] 相机已关闭" << endl;
    return 0;
}
