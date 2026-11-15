#include <iostream>
#include <opencv2/opencv.hpp>
#include "io/camera.hpp"
#include "tools/yaml.hpp"

using namespace std;
using namespace cv;

int main(int argc, char** argv) {
    // 1. 加载相机
    io::Camera camera(PROJECT_ROOT_DIR "/configs/camera.yaml");
    if (!camera.isOpened()) {
        cerr << "相机打开失败！请检查硬件连接或 configs/camera.yaml" << endl;
        return -1;
    }

    // 2. 相机内参
    double fx = 800.0; 
    double fy = 800.0;
    double cx = 640.0;
    double cy = 512.0;
    Mat camera_matrix = (Mat_<double>(3, 3) << fx, 0, cx, 0, fy, cy, 0, 0, 1);
    Mat dist_coeffs = Mat::zeros(5, 1, CV_64F);

    // 3. 定义圆形的真实物理尺寸 (单位：米)
    // ！！！必须改成你实际测量的圆形直径！！！
    double real_diameter = 0.10; // 比如直径 10 厘米
    
    double half = real_diameter / 2.0;
    vector<Point3f> object_points = {
        {(float)-half, (float)-half, 0},
        {(float) half, (float)-half, 0},
        {(float) half, (float) half, 0},
        {(float)-half, (float) half, 0}
    };

    Mat img;
    cout << ">>> 实时圆形测距已启动 <<<" << endl;
    cout << "【操作提示】按键盘上的 'q' 键可退出窗口。" << endl;

    while (true) {
        camera.read(img);
        if (img.empty()) continue;

        // --- 图像处理找圆形 ---
        Mat gray, blur_img;
        cvtColor(img, gray, COLOR_BGR2GRAY);
        GaussianBlur(gray, blur_img, Size(9, 9), 2, 2);

        vector<Vec3f> circles;
        HoughCircles(blur_img, circles, HOUGH_GRADIENT, 1, 50, 100, 30, 20, 500);

        double distance = 0.0;

        if (circles.size() > 0) {
            Vec3f c = circles[0];
            Point center(cvRound(c[0]), cvRound(c[1])); 
            int radius = cvRound(c[2]);

            vector<Point2f> image_points = {
                {(float)(center.x - radius), (float)(center.y - radius)},
                {(float)(center.x + radius), (float)(center.y - radius)},
                {(float)(center.x + radius), (float)(center.y + radius)},
                {(float)(center.x - radius), (float)(center.y + radius)}
            };

            Mat rvec, tvec;
            bool success = solvePnP(object_points, image_points, camera_matrix, dist_coeffs, rvec, tvec);
            
            if (success) {
                distance = tvec.at<double>(2, 0);

                // 画绿圈和红点
                circle(img, center, radius, Scalar(0, 255, 0), 2); 
                circle(img, center, 3, Scalar(0, 0, 255), -1);     

                // 写红字
                string text = format("Dist: %.2f m", distance);
                putText(img, text, Point(center.x - 40, center.y - radius - 10), 
                        FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 0, 255), 2);
                
                // 终端输出
                cout << "测距: " << distance << " 米" << endl;
            }
        }

        // ==========================================
        // 核心：实时弹窗显示
        // ==========================================
        try {
            imshow("Ranging Live", img);
        } catch (const cv::Exception& e) {
            cerr << "弹窗失败，可能是没有显示器或缺少 X11 环境！" << endl;
            break; // 如果弹不出窗口，就退出程序，防止死循环报错
        }

        // 按 'q' 退出
        if (waitKey(1) == 'q') {
            break;
        }
    }

    camera.close();
    return 0;
}