#include "core.hpp"

PixFmt v4l2ToPixFmt(uint32_t fmt) {
    switch (fmt) {
        case V4L2_PIX_FMT_MJPEG: return PixFmt::MJPEG;

        case V4L2_PIX_FMT_YUYV : return PixFmt::YUYV;

        default:                 return PixFmt::FUCKU;
    }
}

uint32_t pixFmtToV4l2(PixFmt fmt) {
    return static_cast<uint32_t>(fmt);
}

std::string pixFmtToStr(PixFmt fmt) {
    switch(fmt) {
        case PixFmt::MJPEG: return "Motion JPEG";
        case PixFmt::YUYV : return "YUYV";
        case PixFmt::FUCKU: return "Unknown";
    }
    __builtin_unreachable();
}

// ---
Camera::Camera(CamInfo info): info(info) {
}

Camera::Camera(Camera&& other) noexcept
    : info(std::move(other.info)),
      dev_fd(other.dev_fd),
      is_open(other.is_open),
      is_streaming(other.isStreaming()) {
    other.dev_fd = -1;
    other.is_open = false;
    other.is_streaming.store(false);
}

Camera& Camera::operator=(Camera&& other) noexcept {
    if (this != &other) {
        this->close();

        this->info = std::move(other.info);
        this->dev_fd = other.dev_fd;
        this->is_open = other.is_open;
        this->is_streaming.store(other.isStreaming());
        
        other.dev_fd = -1;
        other.is_open = false;
        other.is_streaming.store(false);
    }
    
    return *this;
}

Camera::~Camera() {
    this->close();
}

CamInfo Camera::getInfo() const {
    return this->info;
}

bool Camera::open() {
    if (this->is_open) return true;

    this->dev_fd = ::open(this->info.device.c_str(), O_RDWR);
    this->is_open = this->dev_fd >= 0;

    return this->is_open;
}

void Camera::close() {
    if (!this->is_open) return;
    
    if (this->is_streaming.load())
        this->stopStream();

    ::close(this->dev_fd);

    this->is_open = false;
    this->dev_fd = -1;
}

bool Camera::config(size_t fmtI, size_t resI) {
    if (!this->is_open) {
        Log::e() << "Camera isn't open.";
        return false;
    } 

    Format f = this->info.formats[fmtI];
    Size r = f.resolutions[resI];

    Log::d() << pixFmtToStr(f.pix_fmt);

    struct v4l2_format fmt = {0};

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    fmt.fmt.pix.width = std::get<0>(r);
    fmt.fmt.pix.height = std::get<1>(r);
    fmt.fmt.pix.pixelformat = pixFmtToV4l2(f.pix_fmt);
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    return ioctl(this->dev_fd, VIDIOC_S_FMT, &fmt) >= 0;
}

void Camera::startStream(std::function<void(FrameView)> callback) {
    if (!this->is_open) {
        Log::e() << "Camera not open.";
        return;
    }
    if (this->is_streaming.load()) {
        Log::w() << "Already streaming.";
        return;
    }

    this->streamWorker = std::thread(&Camera::streamLoop, this, callback);
}

void Camera::streamLoop(std::function<void(FrameView)> callback) {
    // request buffers
    struct v4l2_requestbuffers req = {0};
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(this->dev_fd, VIDIOC_REQBUFS, &req) < 0) {
        Log::e() << "Failed to request buffers.";
        return;
    }

    // map buffers
    void *buffers[4];

    for (int i = 0; i < req.count; i++) {
        struct v4l2_buffer buf = {0};
        buf.type =  V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(this->dev_fd, VIDIOC_QUERYBUF, &buf) < 0) {
            Log::e() << "Failed to query buffers.";
            return;
        }

        buffers[i] = mmap(
            NULL,
            buf.length,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            this->dev_fd,
            buf.m.offset
        );
    }

    // queue buffers
    for (int i = 0; i < req.count; i++) {
        struct v4l2_buffer buf = {0};
        buf.type =  V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(this->dev_fd, VIDIOC_QBUF, &buf) < 0) {
            Log::e() << "Failed to queue buffers.";
            return;
        }
    }

    // start streaming
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(this->dev_fd, VIDIOC_STREAMON, &type);

    this->is_streaming = true;
    
    // ---
    while (this->is_streaming.load()) { 
        struct timeval timeout = {1, 0};
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(this->dev_fd, &fds);
        
        int r = select(this->dev_fd + 1, &fds, NULL, NULL, &timeout);
        if (r == 0) continue;

        struct v4l2_buffer buf = {0};
        buf.type =  V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(this->dev_fd, VIDIOC_DQBUF, &buf) < 0) {   // dequeue
            if (!this->is_streaming.load())
                break;
            Log::e() << "Failed to dequeue buffers.";
            break;
        }

        callback(FrameView {
            .data = buffers[buf.index],
            .size = buf.bytesused
        }); 
        
        if (ioctl(this->dev_fd, VIDIOC_QBUF, &buf) < 0) {   // dequeue
            if (!this->is_streaming.load())
                break;
            Log::e() << "Failed to requeue buffers.";
            break;
        }
    }
    
    // stop streaming
    if (ioctl(this->dev_fd, VIDIOC_STREAMOFF, &type) < 0) {
        Log::e() << "Failed to stop streaming.";
        return;
    }
    
    // releasing buffers
    req = {0};
    req.count = 0;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(this->dev_fd, VIDIOC_REQBUFS, &req) < 0) {
        Log::e() << "Failed to release buffers.";
        return;
    }
}

void Camera::stopStream() {
    this->is_streaming = false;
    if (this->streamWorker.joinable())
        this->streamWorker.join();
}

bool Camera::isOpen() {
    return this->is_open;
}

bool Camera::isStreaming() {
    return this->is_streaming.load();
}

std::vector<CamInfo> Camera::getCams() {
    std::vector<CamInfo> cameras;

    for (int i = 0; i < 64; i++) {
        // opening device
        char dev[32];
        sprintf(dev, "/dev/video%d", i);

        int f = ::open(dev, O_RDONLY);
        if (f < 0) continue;

        // ---
        CamInfo cam {};

        // getting general metadata
        struct v4l2_capability cap;
        if (ioctl(f, VIDIOC_QUERYCAP, &cap) != 0)
            continue;
        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
            continue;

        cam.device = dev;
        cam.driver = reinterpret_cast<const char*>(cap.driver);
        cam.card = reinterpret_cast<const char*>(cap.card);
        cam.bus = reinterpret_cast<const char*>(cap.bus_info);

        // getting formats
        struct v4l2_fmtdesc fmt = {0};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        while (ioctl(f, VIDIOC_ENUM_FMT, &fmt) == 0) {
            std::string fmt_name = reinterpret_cast<const char*>(fmt.description);
            std::vector<Size> resolutions;

            // getting resolutions
            struct v4l2_frmsizeenum size = {0};
            size.pixel_format = fmt.pixelformat;
            
            while (ioctl(f, VIDIOC_ENUM_FRAMESIZES, &size) == 0) {
                if (size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                    resolutions.push_back({
                        size.discrete.width,
                        size.discrete.height
                    });
                }
                size.index++;
            }

            std::sort(resolutions.begin(), resolutions.end(), [](const Size& a, const Size& b) {
                int areaA = std::get<0>(a) * std::get<1>(a);
                int areaB = std::get<0>(b) * std::get<1>(b);
                return areaA > areaB;
            });
            
            // ---
            cam.formats.push_back(Format {
                .name = fmt_name,
                .pix_fmt = v4l2ToPixFmt(fmt.pixelformat), 
                .resolutions = resolutions
            });
            fmt.index++;
        }

        // getting controls
        struct v4l2_queryctrl ctrl = {0};

        for (ctrl.id = V4L2_CID_BASE; ctrl.id < V4L2_CID_LASTP1; ctrl.id++) {
            if (ioctl(f, VIDIOC_QUERYCTRL, &ctrl) == 0) {
                cam.controls.push_back(Control {
                    .name = reinterpret_cast<const char*>(ctrl.name)
                });
            }
        }

        // ---
        if (cam.formats.size() != 0)
            cameras.push_back(cam);
        
        ::close(f);
    }

    return cameras;
}

std::string Camera::fmt() {
    return Camera::fmtCam(this->info);
}

std::string Camera::fmtCam(const CamInfo& info) {
    std::ostringstream oss;

    oss << "Device : " << info.device << "\n";
    oss << "Driver : " << info.driver << "\n";
    oss << "Card   : " << info.card   << "\n";
    oss << "Bus    : " << info.bus    << "\n";
    oss << "\n[Formats]\n";
    for (const auto& fmt : info.formats) {
        oss << "  - " << fmt.name << " (";
        for (size_t i = 0; i < fmt.resolutions.size(); ++i) {
            oss << std::get<0>(fmt.resolutions[i]) << "x" << std::get<1>(fmt.resolutions[i]);
            if (i < fmt.resolutions.size() - 1) oss << ", ";
        }
        oss << ")\n";
    }
    oss << "\n[Controls]\n";
    for (const auto& ctrl : info.controls) {
        oss << "  - " << ctrl.name << "\n";
    }

    return oss.str();
}

byte* yuyv2rgb(const byte* yuyv, size_t w, size_t h) {
    size_t n_pix = w * h;
    byte* rgb = (byte*)malloc(n_pix * 3);

    if (!rgb) return nullptr;

    size_t j = 0;

    auto clamp = [](int x) {
        return (x < 0) ? 0 : (x > 255 ? 255 : x);
    };

    auto cvt = [rgb, &j, clamp](int y, int u, int v) {
        int c = y - 16;
        int d = u - 128;
        int e = v - 128;

        int r = (298 * c + 409 * e + 128) >> 8;
        int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
        int b = (298 * c + 516 * d + 128) >> 8;

        rgb[j++] = clamp(r);
        rgb[j++] = clamp(g);
        rgb[j++] = clamp(b);
    };

    for (size_t i = 0; i < n_pix * 2;) {
        int y0 = yuyv[i++];
        int u  = yuyv[i++];
        int y1 = yuyv[i++];
        int v  = yuyv[i++];

        cvt(y0, u, v);
        cvt(y1, u, v);
    }

    return rgb;
}