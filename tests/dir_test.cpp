#include <iostream>
#include <opencv2/opencv.hpp>
using namespace std;
using namespace cv;

int main() {
    Mat img = cv::imread(PROJECT_ROOT_DIR "/assets/standard_fanblade.jpg");
    
    if (img.empty()) {
        cout << "ereer" << endl;
        system("pause"); 
        return 0;      
    }
    imshow("text", img);
    cout << "pass" << endl; 
    waitKey(0);//表示等待按键，参数为0表示无限等待，直到有按键按下

    return 0;
}