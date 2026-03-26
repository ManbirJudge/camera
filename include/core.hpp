#ifndef CORE_H
#define CORE_H

#include <string>
#include <vector>
#include <iostream>
#include <cstdio>
#include <tuple>
#include <sstream>
#include <algorithm>
#include <functional>
#include <atomic>
#include <thread>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/videodev2.h>

#include "log.hpp"

typedef u_int8_t byte;
typedef std::tuple<int, int> Size;

typedef struct {
    std::string name;
    u_int32_t pix_fmt;
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

    bool open();
    void close();

    bool config();

    void startStream(std::function<void(FrameView)> callback);
    void stopStream();

    bool isOpen();
    bool isStreaming();

    static std::vector<CamInfo> getCams();
    
    std::string fmt();
    static std::string fmtCam(const CamInfo& info);
};

#endif