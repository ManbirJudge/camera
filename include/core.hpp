#ifndef CORE_H
#define CORE_H

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/videodev2.h>

#include "log.hpp"

typedef uint8_t byte;
typedef std::pair<int, int> Size;

typedef std::pair<uint32_t, std::string> MenuItem;
typedef std::pair<uint32_t, int64_t> IntMenuItem;

enum class PixFmt : uint32_t {
    MJPEG = V4L2_PIX_FMT_MJPEG,
    YUYV  = V4L2_PIX_FMT_YUYV,
    FUCKU = 0
};
PixFmt v4l2ToPixFmt(uint32_t fmt);
uint32_t pixFmtToV4l2(PixFmt fmt);

typedef struct {
    std::string name;
    PixFmt pix_fmt;
    std::vector<Size> resolutions;
} Format;

enum class CtrlType : uint32_t {
    Bool    = V4L2_CTRL_TYPE_BOOLEAN,
    Int     = V4L2_CTRL_TYPE_INTEGER,
    Int64   = V4L2_CTRL_TYPE_INTEGER64,
    Menu    = V4L2_CTRL_TYPE_MENU,
    IntMenu = V4L2_CTRL_TYPE_INTEGER_MENU,
    Unknown = 0
};
CtrlType v4l2ToCtrlType(uint32_t ctrl_type);
uint32_t ctrlTypeToV4l2(CtrlType ctrl_type);
std::string ctrlTypeToStr(CtrlType ctrl_type);

typedef struct {
    uint32_t id;
    CtrlType type;
    std::string name;

    int64_t min;
    int64_t max;
    uint64_t step;
    int64_t default_val;

    std::vector<MenuItem> menu;
    std::vector<std::pair<uint32_t, int64_t>> intMenu;
} Control;

typedef struct {
    int32_t     i32;  // used for - Bool, Int, Menu, IntMenu, Bitmask
    int64_t     i64;
    // std::string str;
} ControlValue;

typedef struct {
    std::string device;
    std::string driver;
    std::string card;
    std::string bus;
    std::vector<Format> formats;
    std::vector<Control> controls;
} CamInfo;

typedef struct {
    void* data;
    size_t size;
} FrameView;

class Camera {  // TODO: throw runtime error if camera moves if its open
private:
    CamInfo info;

    int dev_fd = -1;

    bool is_open = false;
    std::atomic<bool> is_streaming{false};

    std::thread streamWorker;

    void streamLoop(std::function<void(FrameView)> callback);

public:
    
    Camera(CamInfo info);
    Camera(const Camera&) = delete;
    Camera(Camera&& other) noexcept;
    
    ~Camera();

    Camera& operator=(const Camera&) = delete;
    Camera& operator=(Camera&& other) noexcept;

    CamInfo getInfo() const;

    bool open();
    void close();

    bool config(size_t fmtI, size_t resI);

    void startStream(std::function<void(FrameView)> callback);
    void stopStream();

    std::pair<bool, ControlValue> getCtrl(const Control& ctrl);
    bool setCtrl(const Control& ctrl, ControlValue val);

    bool isOpen();
    bool isStreaming();

    static std::vector<CamInfo> getCams();
    
    std::string fmt();
    static std::string fmtCam(const CamInfo& info);
};

void yuyv2rgb(const byte* yuyv, size_t w, size_t h, byte* rgb);

#endif