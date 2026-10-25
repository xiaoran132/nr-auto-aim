#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>

using namespace std;

const float CONF_THRESHOLD = 0.5f;   // 置信度阈值：低于这个分数的框丢弃
const float NMS_THRESHOLD  = 0.4f;   // 非极大值抑制阈值：去掉重叠的重复框

const int   INPUT_SIZE     = 640;    // YOLO 要求的输入边长（正方形）




// class YoloDetector {
//     public:
//         void
//     };



    
int main(int argc, char ** argv){
    // 模型和类别文件路径（可用命令行参数覆盖）
    string xml_path = R"(C:\Users\xwct\Desktop\nr-auto-aim\assents_sp\yolov5.xml)";
    string bin_path = R"(C:\Users\xwct\Desktop\nr-auto-aim\assents_sp\yolov5.bin)";

    
    // 1. 加载 YOLO 模型（OpenVINO IR 格式）
    cv::dnn::Net net;
    try {
        net = cv::dnn::readNetFromModelOptimizer(xml_path, bin_path);
    }
    catch (const cv::Exception& e) {
        cerr << "Error loading model: " << e.what() << endl;//判断模型是否加载成功
        return -1;
    }


    cv::VideoCapture cap(0);

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    
    if (!cap.isOpened()) {
        cerr << "Error_opening camera (index 0)" << endl;
        return -1;
    }

    cv::Mat frame;//创建窗口

    while (true) {
        cap >> frame;                 // 循环读取一帧
        if (frame.empty()){
            cout<<"读取摄像头画面失败"<<endl;
            break;
        }

        cv::Mat blob;
        cv::dnn::blobFromImage(frame, blob, 1.0 / 255.0, cv::Size(640, 640), cv::Scalar(), true, false);

        // 推理
        net.setInput(blob);
        cv::Mat output = net.forward();

        //显示（此处只显示原始画面；接后处理后画框再显示）
        imshow("Camera - press q to quit", frame);

        //（YOLO 需要正方形输入，直接拉伸会变形，所以要等比缩放 + 填充）
        float scale = std::min(
            static_cast<float>(INPUT_SIZE) / frame.cols,
            static_cast<float>(INPUT_SIZE) / frame.rows);
        int new_w = static_cast<int>(frame.cols * scale);
        int new_h = static_cast<int>(frame.rows * scale);
        int pad_x = (INPUT_SIZE - new_w) / 2;
        int pad_y = (INPUT_SIZE - new_h) / 2;


        cv::Mat resized;// letterbox 缩放
            cv::resize(frame, resized, {new_w, new_h});// 填充灰色
            cv::Mat input(INPUT_SIZE, INPUT_SIZE, CV_8UC3, cv::Scalar(114, 114, 114));// 填充灰色
            resized.copyTo(input(cv::Rect(pad_x, pad_y, new_w, new_h)));// 把缩放后的画面拷贝到灰色背景上

            // 转成 blob（归一化到 [0,1]）并送入网络
            blob = cv::dnn::blobFromImage(input, 1.0 / 255.0, {INPUT_SIZE, INPUT_SIZE}, cv::Scalar(), true, false);

            net.setInput(blob);
        

        cv::imshow("yolo5_Out", frame); //显示画面
        if(cv::waitKey(1) == 27) break;//按ESC键退出，esc键的ASCII码是27
    }
    

    //释放摄像头资源
    cap.release();
    cv::destroyAllWindows();
    return 0;
}
    