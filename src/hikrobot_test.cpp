// =====================================================================
// 海康相机取流最小测试: 打开相机 -> 连续抓 N 帧 -> 统计帧率 -> 存一张图
// 不需要 GUI, 用于验证 MVS SDK + 相机透传是否工作。
// 用法: ./hikrobot_test [抓取帧数] [曝光ms] [增益]
// =====================================================================

#include <iostream>
#include <opencv2/opencv.hpp>
#include <string>

#include "hikrobot_camera.hpp"

using namespace std;

int main(int argc, char** argv) {
    int    frames       = (argc > 1) ? atoi(argv[1]) : 30;
    double exposure_ms  = (argc > 2) ? atof(argv[2]) : 2.0;
    double gain         = (argc > 3) ? atof(argv[3]) : 16.0;

    HikRobotCamera cam;
    if (!cam.open(exposure_ms, gain)) {
        cerr << "[FAIL] 打开相机失败" << endl;
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
        string out_path = PROJECT_ROOT_DIR "/hikrobot_test.jpg";
        cv::imwrite(out_path, img);
        cout << "[OK] 已保存首帧到 " << out_path << endl;
    }

    cam.close();
    cout << "[OK] 相机已关闭" << endl;
    return 0;
}
