#include<iostream>
#include<opencv2/opencv.hpp>
#include<opencv2/dnn.hpp>
#include<sstream>
#include<iomanip>

using namespace std;

int main(){

//打开摄像头，0表示默认摄像头，如果有多个摄像头，可以尝试1、2等
    cv::VideoCapture cap(1);
    cv::VideoCapture cap1(0);

//设置摄像头分辨率
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    cap1.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap1.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

//判断摄像头是否打开成功
    if(!cap.isOpened()){
        cout<<"摄像头0打开失败"<<endl;
        return -1;
    }

    if(!cap1.isOpened()){
        cout<<"摄像头1打开失败"<<endl;
        return -1;
    }

    //创建窗口
    cv::Mat frame1;
    cv::Mat frame;

    double fps1 = 0.0;
    double fps = 0.0;

    double T_start1 = (double)cv::getTickCount(); //获取当前时间戳
    double T_start = (double)cv::getTickCount(); //获取当前时间戳

    while(true){
        cap>>frame; //读取摄像头画面
        cap1>>frame1; //读取摄像头1画面
        if (frame.empty()){
            cout<<"读取摄像头画面失败"<<endl;
            break;
        }
        if (frame1.empty()){
            cout<<"读取摄像头1画面失败"<<endl;
            break;
        }

        double T_end1 = (double)cv::getTickCount(); //获取当前时间戳
        double T_end = (double)cv::getTickCount(); //获取当前时间戳

        double time1 = (T_end1 - T_start1) / cv::getTickFrequency(); //计算时间间隔，单位为秒
        double time = (T_end - T_start) / cv::getTickFrequency(); //计算时间间隔，单位为秒
        
        if (time1 >= 0) { //每秒更新一次FPS
            fps1 = 0.9 * fps1 + 0.1 * (1.0 / time1); //计算FPS
            // fps = 1.0 / time;这个是每帧的FPS，波动很大，不适合显示
        }

        if (time >= 0) { //每秒更新一次FPS
            fps = 0.9 * fps + 0.1 * (1.0 / time); //计算FPS
        }
        T_start = T_end; //更新时间戳
        T_start1 = T_end1; //更新时间戳
    cv::putText(frame,
                        "Gay",
                        cv::Point(20, 40),              // 文字左下角坐标
                        cv::FONT_HERSHEY_SIMPLEX,       // 字体
                        1.0,                            // 字体缩放
                        cv::Scalar(0, 255, 0),          // 颜色 (B, G, R) -> 绿色
                        2,                              // 线宽
                        cv::LINE_AA);                   // 抗锯齿 2);
                        
    cv::putText(frame1,
                        "Gay",
                        cv::Point(20, 40),              // 文字左下角坐标
                        cv::FONT_HERSHEY_SIMPLEX,       // 字体
                        1.0,                            // 字体缩放
                        cv::Scalar(0, 255, 0),          // 颜色 (B, G, R) -> 绿色
                        2,                              // 线宽
                        cv::LINE_AA);                   // 抗锯齿 2);

    std::stringstream ss;
        ss << "FPS: " << std::fixed << std::setprecision(1) << fps;
    
    std::stringstream ss1;
        ss1 << "FPS: " << std::fixed << std::setprecision(1) << fps1;

    cv::putText(frame,
                        ss.str(),
                        cv::Point(20, 80),
                        cv::FONT_HERSHEY_SIMPLEX,
                        1.0,
                        cv::Scalar(0, 0, 255),          // 红色
                        2,
                        cv::LINE_AA);
                        
    cv::putText(frame1,
                        ss1.str(),
                        cv::Point(20, 80),
                        cv::FONT_HERSHEY_SIMPLEX,
                        1.0,
                        cv::Scalar(0, 0, 255),          // 红色
                        2,
                        cv::LINE_AA);

        cv::imshow("00000", frame); //显示画面
        cv::imshow("111111", frame1); //显示画面

        if(cv::waitKey(1) == 27) //按ESC键退出，esc键的ASCII码是27
            b
    //释放摄像头资源
    cap.release();
    cap1.release();
    cv::destroyAllWindows();
    return 0;
    //
}