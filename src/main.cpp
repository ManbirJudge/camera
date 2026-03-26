#include "main.hpp" 

std::unique_ptr<Camera> g_cam;

std::mutex g_frame_mtx;
std::vector<byte> g_frame;

std::atomic<bool> g_cap;

void on_wnd_close(Fl_Widget *w, void *data) {
    g_cam->close();
    w->hide();
}

void on_frame_received(void* userData) {
    Fl_Box* canvas = (Fl_Box*)userData;
    std::vector<byte> frame;

    {
        std::lock_guard<std::mutex> lock(g_frame_mtx);
        frame = g_frame;
    }

    int w, h, ch;

    byte *rgb = stbi_load_from_memory(
        frame.data(),
        frame.size(),
        &w, &h,
        &ch,
        3
    );
    if (!rgb) return;

    Fl::lock();

    int canvas_w = canvas->w();
    int canvas_h = canvas->h();

    float img_ratio = (float)w / h;
    float canvas_ratio = (float)canvas_w / canvas_h;

    int new_w, new_h;

    if (img_ratio > canvas_ratio) {
        new_w = canvas_w;
        new_h = canvas_w / img_ratio;
    } else {
        new_h = canvas_h;
        new_w = canvas_h * img_ratio;
    }

    Fl_RGB_Image* img = new Fl_RGB_Image(rgb, w, h, 3);
    Fl_RGB_Image* scaled_copy = (Fl_RGB_Image*)img->copy(new_w, new_h);

    stbi_image_free(rgb);
    delete img;
    
    if (canvas->image())
        delete canvas->image();
    canvas->image(scaled_copy);
    canvas->redraw();

        Fl::unlock();
}

void on_cap_btn_click(Fl_Widget *w, void *data) {
    g_cap = true;
}

int main(int argc, char** argv) {
    Settings settings;

    Fl::lock();

    // --- ui
    // window
    Fl_Double_Window *wnd = new Fl_Double_Window(400, 300, "Camera");

    // root
    Fl_Flex *root = new Fl_Flex(0, 0, 400, 300, Fl_Flex::VERTICAL);
    root->gap(8);

    // toolbar
    // Fl_Flex *toolbar = new Fl_Flex(0, 0, 0, 0, Fl_Flex::HORIZONTAL);
    // toolbar->gap(8);

    // Fl_Choice *cam_sel = new Fl_Choice(0, 0, 0, 0);
    // Fl_Choice *fmt_sel = new Fl_Choice(0, 0, 0, 0);
    // Fl_Choice *res_sel = new Fl_Choice(0, 0, 0, 0);
    // new Fl_Box(0, 0, 0, 0);
    // Fl_Button *settings_btn = new Fl_Button(0, 0, 0, 0, "⚙️");

    // toolbar->fixed(settings_btn, 40);
    // toolbar->end();
    
    // canvas
    Fl_Box *canvas = new Fl_Box(0, 0, 0, 0);

    // controls
    Fl_Flex *controls = new Fl_Flex(0, 0, 0, 0, Fl_Flex::HORIZONTAL);
    controls->gap(8);

    new Fl_Box(0, 0, 0, 0);

    Fl_Button *cap_btn = new Fl_Button(0, 0, 0, 0, "Capture");
    cap_btn->callback(on_cap_btn_click);
    
    new Fl_Box(0, 0, 0, 0);

    controls->fixed(cap_btn, 96);
    controls->end();

    // finalize
    // root->fixed(toolbar, 32);
    root->fixed(controls, 32);
    root->end();

    wnd->resizable(controls);
    wnd->end();

    wnd->callback(on_wnd_close);
    wnd->show(argc, argv);

    // init
    std::vector<CamInfo> cameras = Camera::getCams();

    // std::string _ = Utils::join<CamInfo>(cameras, "|", [](const CamInfo& info) {
    //     return info.card;
    // });
    // cam_sel->add(_.c_str());
    // _ = Utils::join<Format>(cameras[0].formats, "|", [](const Format& fmt) {
    //     return fmt.name;
    // });
    // fmt_sel->add(_.c_str());
    // _ = Utils::join<Size>(cameras[0].formats[0].resolutions, "|", [](const Size& size) {
    //     return std::to_string(std::get<0>(size)) + "x" + std::to_string(std::get<1>(size));
    // });
    // res_sel->add(_.c_str());

    Log::d() << "Found " << cameras.size() << " cameras:";
    for (size_t i = 0; i < cameras.size(); i++) {
        std::cout << i << " ------------------" << std::endl;
        std::cout << Camera::fmtCam(cameras[i]) << std::endl;
    }

    g_cam = std::unique_ptr<Camera>(new Camera(cameras[cameras.size() - 1]));

    g_cam->open();
    g_cam->config();
    g_cam->startStream([canvas, settings](FrameView fv) {
        {
            std::lock_guard<std::mutex> lock(g_frame_mtx);

            g_frame.resize(fv.size);
            g_frame.assign((byte*)fv.data, (byte*)fv.data + fv.size);
        }

        if (g_cap.exchange(false)) {
            std::string filePath = settings.out_dir + "/" + Utils::getFormattedDt(settings.filename_fmt) + ".jpg";

            FILE *f = fopen(filePath.c_str(), "wb");
            if (f) {
                fwrite(fv.data, fv.size, 1, f);
                fclose(f);
            }

            Log::d() << "Saved to: " << filePath;
        }

        Fl::awake(on_frame_received, canvas);
    });
    
    // ---
    return Fl::run();
}