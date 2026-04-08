#include "core.hpp"

PixFmt v4l2ToPixFmt(uint32_t _fmt_desc) {
    switch (_fmt_desc) {
        case V4L2_PIX_FMT_MJPEG: return PixFmt::MJPEG;
        case V4L2_PIX_FMT_YUYV : return PixFmt::YUYV ;
        default                : return PixFmt::FUCKU;
    }
}

uint32_t pixFmtToV4l2(PixFmt _fmt_desc) {
    return static_cast<uint32_t>(_fmt_desc);
}

CtrlType v4l2ToCtrlType(uint32_t ctrl_type) {
    switch (ctrl_type) {
        case V4L2_CTRL_TYPE_BOOLEAN     : return CtrlType::Bool;
        case V4L2_CTRL_TYPE_INTEGER     : return CtrlType::Int;
        case V4L2_CTRL_TYPE_INTEGER64   : return CtrlType::Int64;
        case V4L2_CTRL_TYPE_MENU        : return CtrlType::Menu;
        case V4L2_CTRL_TYPE_INTEGER_MENU: return CtrlType::IntMenu;
        // case V4L2_CTRL_TYPE_BUTTON      : return CtrlType::Btn;
        // case V4L2_CTRL_TYPE_BITMASK     : return CtrlType::Bitmask;
        // case V4L2_CTRL_TYPE_STRING      : return CtrlType::Str;
        default                         : return CtrlType::Unknown;
    }
}

uint32_t ctrlTypeToV4l2(CtrlType ctrl_type) {
    return static_cast<uint32_t>(ctrl_type);
}

std::string ctrlTypeToStr(CtrlType ctrl_type) {
    switch (ctrl_type) {
        case CtrlType::Bool   : return "bool";
        case CtrlType::Int    : return "int";
        case CtrlType::Int64  : return "int64";
        case CtrlType::Menu   : return "menu";
        case CtrlType::IntMenu: return "integer menu";
        // case CtrlType::Btn    : return "button";
        // case CtrlType::Bitmask: return "bitmask";
        // case CtrlType::Str    : return "string";
        case CtrlType::Unknown: return "unknown";
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

    struct v4l2_format _fmt = {0};

    _fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    _fmt.fmt.pix.width = std::get<0>(r);
    _fmt.fmt.pix.height = std::get<1>(r);
    _fmt.fmt.pix.pixelformat = pixFmtToV4l2(f.pix_fmt);
    _fmt.fmt.pix.field = V4L2_FIELD_NONE;

    return ioctl(this->dev_fd, VIDIOC_S_FMT, &_fmt) >= 0;
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
    size_t buf_lens[4];

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
        buf_lens[i] = buf.length;
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

    // un-mapping buffers
    for (int i = 0; i < req.count; i++) {
        munmap(
            buffers[i],
            buf_lens[i]
        );
    }
    
    // releasing buffers
    struct v4l2_requestbuffers release_req = {0};
    release_req.count = 0;
    release_req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    release_req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(this->dev_fd, VIDIOC_REQBUFS, &release_req) < 0) {
        Log::e() << "Failed to release buffers.";
        return;
    }
}

void Camera::stopStream() {
    this->is_streaming = false;
    if (this->streamWorker.joinable())
        this->streamWorker.join();
}

std::pair<bool, ControlValue> Camera::getCtrl(const Control& ctrl) {
    ControlValue val{0};

    if (!this->is_open) {
        Log::e() << "Camera not open.";
        return { false, val };
    }
    
    struct v4l2_ext_control _ctrl {0};
    struct v4l2_ext_controls _ctrls {0};

    _ctrl.id = ctrl.id;

    _ctrls.count = 1;
    _ctrls.controls = &_ctrl;

    if (ioctl(this->dev_fd, VIDIOC_G_EXT_CTRLS, &_ctrls) != 0)
        return { false, val };

    switch (ctrl.type)
    {
    case CtrlType::Bool:
    case CtrlType::Int:
    case CtrlType::Menu:
    case CtrlType::IntMenu:
        val.i32 = _ctrl.value;
        break;
    case CtrlType::Int64:
        val.i64 = _ctrl.value64;
        break;
    default:
        break;
    }

    return { true, val };
}

bool Camera::setCtrl(const Control& ctrl, ControlValue val) {
    if (!this->is_open) {
        Log::e() << "Camera not open.";
        return false; 
    }
 
    struct v4l2_ext_control _ctrl {0};
    struct v4l2_ext_controls _ctrls {0};

    _ctrl.id = ctrl.id;

    _ctrls.count = 1;
    _ctrls.controls = &_ctrl;

    switch (ctrl.type)
    {
    case CtrlType::Bool:
    case CtrlType::Int:
    case CtrlType::Menu:
    case CtrlType::IntMenu:
        _ctrl.value = val.i32;
        break;
    case CtrlType::Int64:
        _ctrl.value64 = val.i64;
        break;
    default:
        break;
    }

    return ioctl(this->dev_fd, VIDIOC_S_EXT_CTRLS, &_ctrls) != 0;
}

bool Camera::isOpen() {
    return this->is_open;
}

bool Camera::isStreaming() {
    return this->is_streaming.load();
}

std::vector<CamInfo> Camera::getCams() {
    std::vector<CamInfo> cameras;

    for (int i = 0; i < 256; i++) {
        // opening device
        char dev[32];
        sprintf(dev, "/dev/video%d", i);

        int f = ::open(dev, O_RDONLY);
        if (f < 0) continue;

        // getting general metadata
        struct v4l2_capability _capability;
        if (ioctl(f, VIDIOC_QUERYCAP, &_capability) != 0) continue;
        if (!(_capability.capabilities & V4L2_CAP_VIDEO_CAPTURE)) continue;

        CamInfo cam {};

        cam.device = dev;
        cam.driver = reinterpret_cast<const char*>(_capability.driver);
        cam.card = reinterpret_cast<const char*>(_capability.card);
        cam.bus = reinterpret_cast<const char*>(_capability.bus_info);

        // getting formats
        struct v4l2_fmtdesc _fmt_desc = {0};
        _fmt_desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        while (ioctl(f, VIDIOC_ENUM_FMT, &_fmt_desc) == 0) {
            // getting resolutions
            std::vector<Size> resolutions;

            struct v4l2_frmsizeenum _size = {0};
            _size.pixel_format = _fmt_desc.pixelformat;
            
            while (ioctl(f, VIDIOC_ENUM_FRAMESIZES, &_size) == 0) {
                if (_size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                    resolutions.push_back({
                        _size.discrete.width,
                        _size.discrete.height
                    });
                }
                _size.index++;
            }

            std::sort(resolutions.begin(), resolutions.end(), [](const Size& a, const Size& b) {
                int areaA = a.first * a.second;
                int areaB = b.first * b.second;
                return areaA > areaB;
            });
            
            // ---
            cam.formats.push_back(Format {
                .name = reinterpret_cast<const char*>(_fmt_desc.description),
                .pix_fmt = v4l2ToPixFmt(_fmt_desc.pixelformat), 
                .resolutions = resolutions
            });
            _fmt_desc.index++;
        }

        // getting controls
        struct v4l2_query_ext_ctrl _ctrl = {0};
        _ctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;

        while (ioctl(f, VIDIOC_QUERY_EXT_CTRL, &_ctrl) == 0) {
            if (!(_ctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS || (_ctrl.flags & V4L2_CTRL_FLAG_DISABLED))) {
                Control ctrl{
                    .id = _ctrl.id,
                    .type = v4l2ToCtrlType(_ctrl.type),
                    .name = std::string(reinterpret_cast<const char*>(_ctrl.name)),

                    .min = _ctrl.minimum,
                    .max = _ctrl.maximum,
                    .step = _ctrl.step,
                    .default_val = _ctrl.default_value
                };

                if (ctrl.type == CtrlType::Menu) {
                    struct v4l2_querymenu _menu {0};
                    _menu.id = ctrl.id;

                    for (int j = ctrl.min; j <= ctrl.max; j++) {
                        _menu.index = j;

                        if (ioctl(f, VIDIOC_QUERYMENU, &_menu) == 0) {
                            ctrl.menu.push_back(MenuItem {
                                j,
                                std::string(reinterpret_cast<const char*>(_menu.name))
                            });
                        } else {
                            if (errno == EINVAL) continue;  // invalid index -> skip
                            break;                          // something bad
                        }
                    }
                }

                cam.controls.push_back(ctrl);
            }
            
            _ctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
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

    oss << "Device: " << info.device << "\n";
    oss << "Driver: " << info.driver << "\n";
    oss << "Card  : " << info.card   << "\n";
    oss << "Bus   : " << info.bus    << "\n";
    oss << "\n[Formats]\n";
    for (const auto& _fmt_desc : info.formats) {
        oss << "  - " << _fmt_desc.name << " (";
        for (size_t i = 0; i < _fmt_desc.resolutions.size(); ++i) {
            oss << std::get<0>(_fmt_desc.resolutions[i]) << "x" << std::get<1>(_fmt_desc.resolutions[i]);
            if (i < _fmt_desc.resolutions.size() - 1) oss << ", ";
        }
        oss << ")\n";
    }
    oss << "\n[Controls]\n";
    for (size_t i = 0, n = info.controls.size(); i < n; i++) {
        const Control ctrl = info.controls[i];
        oss << "  - " << ctrl.name << " (" << ctrlTypeToStr(ctrl.type) << ')';
        if (ctrl.type == CtrlType::Menu) {
            oss << "\n      Options: ";
            for (auto entry : ctrl.menu) oss << entry.second << ", ";
        }
        if (i < n - 1) oss << "\n";
    }

    return oss.str();
}

void yuyv2rgb(const byte* yuyv, size_t w, size_t h, byte* rgb) {
    size_t n_pix = w * h;
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
}