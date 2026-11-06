// =====================================================================
// 海康机器人(HIKROBOT) USB3.0 机器视觉相机封装 —— 实现
//   接口见 hikrobot.hpp
// =====================================================================

#include "hikrobot.hpp"

#include <cstdio>
#include <string>

#include "MvCameraControl.h"
#include "tools/yaml.hpp"

HikRobotCamera::HikRobotCamera()
: handle_(nullptr), grabbing_(false)
{
}

HikRobotCamera::~HikRobotCamera()
{
    close();
}

bool HikRobotCamera::open(double exposure_ms, double gain)
{
    unsigned int ret = 0;

    MV_CC_DEVICE_INFO_LIST device_list{};
    ret = MV_CC_EnumDevices(MV_USB_DEVICE, &device_list);
    if (ret != MV_OK || device_list.nDeviceNum == 0) {
        std::fprintf(stderr,
                     "[HikRobot] 未找到 USB 相机 (MV_CC_EnumDevices=0x%x, 设备数=%u)\n",
                     ret, device_list.nDeviceNum);
        return false;
    }

    ret = MV_CC_CreateHandle(&handle_, device_list.pDeviceInfo[0]);
    if (ret != MV_OK) {
        std::fprintf(stderr, "[HikRobot] MV_CC_CreateHandle 失败: 0x%x\n", ret);
        return false;
    }

    ret = MV_CC_OpenDevice(handle_);
    if (ret != MV_OK) {
        std::fprintf(stderr, "[HikRobot] MV_CC_OpenDevice 失败: 0x%x\n"
                             "          (若提示权限/打开失败, 可能需要 udev 规则或 sudo)\n", ret);
        MV_CC_DestroyHandle(handle_);
        handle_ = nullptr;
        return false;
    }

    // 自动白平衡开启; 自动曝光/自动增益关闭, 改成手动设定(保证画面稳定)
    MV_CC_SetEnumValue(handle_, "BalanceWhiteAuto", MV_BALANCEWHITE_AUTO_CONTINUOUS);
    MV_CC_SetEnumValue(handle_, "ExposureAuto", MV_EXPOSURE_AUTO_MODE_OFF);
    MV_CC_SetEnumValue(handle_, "GainAuto", MV_GAIN_MODE_OFF);
    MV_CC_SetFloatValue(handle_, "ExposureTime", exposure_ms * 1000.0);  // 单位 us
    MV_CC_SetFloatValue(handle_, "Gain", gain);

    ret = MV_CC_StartGrabbing(handle_);
    if (ret != MV_OK) {
        std::fprintf(stderr, "[HikRobot] MV_CC_StartGrabbing 失败: 0x%x\n", ret);
        MV_CC_CloseDevice(handle_);
        MV_CC_DestroyHandle(handle_);
        handle_ = nullptr;
        return false;
    }

    grabbing_ = true;
    return true;
}

bool HikRobotCamera::open(const std::string& config_path)
{
    try {
        const YAML::Node yaml = tools::load(config_path);

        const std::string camera_name = tools::read<std::string>(yaml, "camera_name");
        if (camera_name != "hikrobot") {
            std::fprintf(stderr,
                         "[HikRobot] 配置中的 camera_name 为 \"%s\"，不是 hikrobot\n",
                         camera_name.c_str());
            return false;
        }

        const double exposure_ms = tools::read<double>(yaml, "exposure_ms");
        const double gain        = tools::read<double>(yaml, "gain");
        // vid_pid 预留给后续按 VID/PID 选择设备，当前封装打开第一台 USB 相机
        return open(exposure_ms, gain);
    }
    catch (const std::exception& e) {
        std::fprintf(stderr, "[HikRobot] 读取配置失败: %s\n", e.what());
        return false;
    }
}

bool HikRobotCamera::read(cv::Mat& bgr)
{
    if (!grabbing_ || !handle_) return false;

    MV_FRAME_OUT raw;
    unsigned int nMsec = 1000;  // 取图超时 1s
    unsigned int ret = MV_CC_GetImageBuffer(handle_, &raw, nMsec);
    if (ret != MV_OK) {
        std::fprintf(stderr, "[HikRobot] MV_CC_GetImageBuffer 失败: 0x%x\n", ret);
        return false;
    }

    const int w = raw.stFrameInfo.nWidth;
    const int h = raw.stFrameInfo.nHeight;
    const MvGvspPixelType pt = raw.stFrameInfo.enPixelType;

    if (pt == PixelType_Gvsp_BayerGR8 || pt == PixelType_Gvsp_BayerRG8 ||
        pt == PixelType_Gvsp_BayerGB8 || pt == PixelType_Gvsp_BayerBG8) {
        // 单通道 Bayer 原始数据 -> BGR
        cv::Mat bayer(h, w, CV_8UC1, raw.pBufAddr);
        int code = 0;
        if (pt == PixelType_Gvsp_BayerGR8)      code = cv::COLOR_BayerGR2BGR;
        else if (pt == PixelType_Gvsp_BayerRG8) code = cv::COLOR_BayerRG2BGR;
        else if (pt == PixelType_Gvsp_BayerGB8) code = cv::COLOR_BayerGB2BGR;
        else                                    code = cv::COLOR_BayerBG2BGR;
        cv::cvtColor(bayer, bgr, code);
    }
    else if (pt == PixelType_Gvsp_Mono8) {
        cv::Mat gray(h, w, CV_8UC1, raw.pBufAddr);
        cv::cvtColor(gray, bgr, cv::COLOR_GRAY2BGR);
    }
    else if (pt == PixelType_Gvsp_BGR8_Packed) {
        bgr = cv::Mat(h, w, CV_8UC3, raw.pBufAddr).clone();
    }
    else if (pt == PixelType_Gvsp_RGB8_Packed) {
        cv::Mat rgb(h, w, CV_8UC3, raw.pBufAddr);
        cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
    }
    else {
        // 其余格式交给 SDK 的像素格式转换
        MV_CC_PIXEL_CONVERT_PARAM cvt{};
        cvt.nWidth = w;
        cvt.nHeight = h;
        cvt.pSrcData = raw.pBufAddr;
        cvt.nSrcDataLen = raw.stFrameInfo.nFrameLen;
        cvt.enSrcPixelType = pt;
        bgr.create(h, w, CV_8UC3);
        cvt.pDstBuffer = bgr.data;
        cvt.nDstBufferSize = bgr.total() * bgr.elemSize();
        cvt.enDstPixelType = PixelType_Gvsp_BGR8_Packed;
        MV_CC_ConvertPixelType(handle_, &cvt);
    }

    MV_CC_FreeImageBuffer(handle_, &raw);
    return true;
}

void HikRobotCamera::close()
{
    if (handle_) {
        if (grabbing_) {
            MV_CC_StopGrabbing(handle_);
            grabbing_ = false;
        }
        MV_CC_CloseDevice(handle_);
        MV_CC_DestroyHandle(handle_);
        handle_ = nullptr;
    }
}

bool HikRobotCamera::isOpened() const
{
    return handle_ != nullptr && grabbing_;
}
