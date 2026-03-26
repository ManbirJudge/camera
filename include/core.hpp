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
#include <tuple>
#include <vector>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/videodev2.h>

#include "log.hpp"

typedef uint8_t byte;
typedef std::tuple<int, int> Size;

enum class PixFmt : uint32_t {
    MJPEG = V4L2_PIX_FMT_MJPEG,

    YUYV  = V4L2_PIX_FMT_YUYV ,

    FUCKU = 0
};
PixFmt v4l2ToPixFmt(uint32_t fmt);
uint32_t pixFmtToV4l2(PixFmt fmt);
std::string pixFmtToStr(PixFmt fmt);

typedef struct {
    std::string name;
    PixFmt pix_fmt;
    std::vector<Size> resolutions;
} Format;

typedef struct {
    std::string name;
} Control;

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

    bool isOpen();
    bool isStreaming();

    static std::vector<CamInfo> getCams();
    
    std::string fmt();
    static std::string fmtCam(const CamInfo& info);
};

byte* yuyv2rgb(const byte* yuyv, size_t w, size_t h);

#endif