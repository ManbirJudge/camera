#include "main.hpp" 

MainWindow::MainWindow() : cameras(Camera::getCams()) {
    // Log::d() << "Found " << cameras.size() << " cameras:";
    // for (size_t i = 0; i < cameras.size(); i++) {
    //     std::cout << i << " ------------------" << std::endl;
    //     std::cout << Camera::fmtCam(cameras[i]) << std::endl;
    // }

    Fl::lock();

    this->initUi();
}

void MainWindow::initUi() {
    // window
    this->wnd = new Fl_Double_Window(500, 400, "Camera");

    // root
    Fl_Flex *root = new Fl_Flex(0, 0, 500, 400, Fl_Flex::VERTICAL);
    root->margin(UI_GAP);
    root->gap(UI_GAP);

    // toolbar
    Fl_Flex *toolbar = new Fl_Flex(0, 0, 0, 0, Fl_Flex::HORIZONTAL);
    toolbar->gap(UI_GAP);

    this->_cam_sel = new Fl_Choice(0, 0, 0, 0);
    this->_fmt_sel = new Fl_Choice(0, 0, 0, 0);
    this->_res_sel = new Fl_Choice(0, 0, 0, 0);
    new Fl_Box(0, 0, 0, 0);
    Fl_Button* controls_btn = new Fl_Button(0, 0, 0, 0, "Controls");

    this->_cam_sel->callback(MainWindow::_onCamSelChange, this);
    this->_fmt_sel->callback(MainWindow::_onFmtSelChange, this);
    this->_res_sel->callback(MainWindow::_onResSelChange, this);
    controls_btn->callback(MainWindow::_onCtrlsBtnClick, this);

    toolbar->fixed(this->_cam_sel, 112);
    toolbar->fixed(this->_fmt_sel, 112);
    toolbar->fixed(this->_res_sel, 112);
    toolbar->fixed(controls_btn,   96 );
    toolbar->end();
    
    // canvas
    this->_canvas = new Fl_Box(0, 0, 0, 0);

    // controls
    Fl_Flex *controls = new Fl_Flex(0, 0, 0, 0, Fl_Flex::HORIZONTAL);
    controls->gap(UI_GAP);

    new Fl_Box(0, 0, 0, 0);

    Fl_Button *cap_btn = new Fl_Button(0, 0, 0, 0, "Capture");
    cap_btn->callback(MainWindow::_onCapBtnClick, this);
    
    new Fl_Box(0, 0, 0, 0);

    controls->fixed(cap_btn, 96);
    controls->end();

    // finalize
    root->fixed(toolbar, 32);
    root->fixed(controls, 32);
    root->end();

    this->wnd->resizable(controls);
    this->wnd->end();

    this->wnd->callback(MainWindow::_onWndClose, this);

    // data
    std::string _ = Utils::join<CamInfo>(cameras, "|", [](const CamInfo& info) {
        return info.card;
    });
    this->_cam_sel->add(_.c_str());
}

void MainWindow::show() {
    if (this->cameras.size() > 0) {
        this->_cam_sel->value(0);
        this->onCamSelChange();
    }
    this->wnd->show();
}

// event wrappers
void MainWindow::_onWndClose(Fl_Widget* w, void* data) {
    MainWindow* inst = static_cast<MainWindow*>(data);
    inst->onWndClose();
}

void MainWindow::_onFrameReceived(void* data) {
    MainWindow* inst = static_cast<MainWindow*>(data);
    inst->onFrameReceived();
}

void MainWindow::_onCapBtnClick(Fl_Widget* w, void* data) {
    MainWindow* inst = static_cast<MainWindow*>(data);
    inst->onCapBtnClick();
}

void MainWindow::_onCamSelChange(Fl_Widget* w, void* data) {
    MainWindow* inst = static_cast<MainWindow*>(data);
    inst->onCamSelChange();
}

void MainWindow::_onFmtSelChange(Fl_Widget* w, void* data) {
    MainWindow* inst = static_cast<MainWindow*>(data);
    inst->onFmtSelChange();
}

void MainWindow::_onResSelChange(Fl_Widget* w, void* data) {
    MainWindow* inst = static_cast<MainWindow*>(data);
    inst->onResSelChange();
}

void MainWindow::_onCtrlsBtnClick(Fl_Widget* w, void* data) {
    MainWindow* inst = static_cast<MainWindow*>(data);
    inst->onCtrlsBtnClick();
}

void MainWindow::_camCtrlCallback(Fl_Widget* w, void* data) {
    CtrlCallbackData* cbData = static_cast<CtrlCallbackData*>(data);
    MainWindow* wnd = cbData->wnd;

    int32_t val = 0;
    if (Fl_Value_Slider* s = dynamic_cast<Fl_Value_Slider*>(w)) {
        val = (int32_t)s->value();
    } else if (Fl_Check_Button* cb = dynamic_cast<Fl_Check_Button*>(w)) {
        val = cb->value();
    }

    wnd->camCtrlCallback(cbData->ctrl_id, val);

    // delete cbData;
}

// event handlers
void MainWindow::onWndClose() {
    if (this->cam != nullptr)
        this->cam->close();
    this->wnd->hide();
}

void MainWindow::onFrameReceived() {
    std::vector<byte> _frame;

    {
        std::lock_guard<std::mutex> lock(this->frame_mtx);
        _frame = this->frame;
    }

    int w, h, ch;  // TODO: get from somewhere trustworthy
    byte *rgb = nullptr;
    
    const Format fmt = this->cam->getInfo().formats[this->_fmt_sel->value()];
    switch (fmt.pix_fmt) {
        case PixFmt::MJPEG: {
            rgb = stbi_load_from_memory(
                _frame.data(),
                _frame.size(),
                &w, &h,
                &ch,
                3
            );
            break;
        }
        case PixFmt::YUYV: {
            const Size res = fmt.resolutions[this->_res_sel->value()];
            w = std::get<0>(res);
            h = std::get<1>(res);
            ch = 3;

            rgb = yuyv2rgb(_frame.data(), w, h);
            
            break;
        }
        case PixFmt::FUCKU: {
            Log::e() << "Unknown pixel format.";
            break;
        }
    } 
    if (!rgb) {
        frame_pending = false;
        return;
    }

    int canvas_w = this->_canvas->w();
    int canvas_h = this->_canvas->h();

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
    
    if (this->_canvas->image()) delete this->_canvas->image();
    this->_canvas->image(scaled_copy);
    this->_canvas->redraw();

    this->frame_pending = false;
}

void MainWindow::onCapBtnClick() {
    this->should_cap = true;
}

void MainWindow::onCamSelChange() {
    if (this->cam != nullptr)
        this->cam->close();
   
    this->cam = std::unique_ptr<Camera>(new Camera(this->cameras[this->_cam_sel->value()]));

    const CamInfo info = this->cam->getInfo();
    std::string _ = Utils::join<Format>(info.formats, "|", [](const Format& fmt) {
        return fmt.name;
    });
    this->_fmt_sel->clear();
    this->_fmt_sel->add(_.c_str());
    this->_fmt_sel->value(0);
    const Format fmt = info.formats[0];
    _ = Utils::join<Size>(fmt.resolutions, "|", [](const Size& size) {
        return std::to_string(std::get<0>(size)) + "x" + std::to_string(std::get<1>(size));
    });
    this->_res_sel->clear();
    this->_res_sel->add(_.c_str());
    this->_res_sel->value(0);

    this->cam->open();
    this->configAndStartCamStream();
}

void MainWindow::onFmtSelChange() {
    if (this->cam == nullptr) return;
    
    this->cam->stopStream();

    const CamInfo info = this->cam->getInfo();
    const Format fmt = info.formats[this->_fmt_sel->value()];
    std::string _ = Utils::join<Size>(fmt.resolutions, "|", [](const Size& size) {
        return std::to_string(std::get<0>(size)) + "x" + std::to_string(std::get<1>(size));
    });
    this->_res_sel->clear();
    this->_res_sel->add(_.c_str());
    this->_res_sel->value(0);

    this->configAndStartCamStream();
}

void MainWindow::onResSelChange() {
    if (this->cam == nullptr) return;
    
    this->cam->stopStream();

    this->configAndStartCamStream();
}

void MainWindow::onCtrlsBtnClick() {
    if (this->cam == nullptr) return;

    Fl_Window* ctrls_wnd = new Fl_Window(400, 400, "Controls");

    Fl_Grid* grid = new Fl_Grid(
        0, 0,
        400, 400
    );
    grid->layout((int)this->cam->getInfo().controls.size(), 2, UI_GAP, UI_GAP);
    grid->col_width(0, 150);
    grid->col_width(1, 250);

    size_t i = 0;
    for (const Control& ctrl : this->cam->getInfo().controls) {
        Fl_Box* lbl = new Fl_Box(0, 0, 1, 1);
        lbl->copy_label(ctrl.name.c_str());
        lbl->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

        CtrlCallbackData* cbData = new CtrlCallbackData{ this, ctrl.id };  // TODO: delete this somewhere

        Fl_Widget* w = nullptr;
        switch (ctrl.type)
        {
        case CtrlType::Int: {
            Fl_Value_Slider* s = new Fl_Value_Slider(0, 0, 1, 1);
            s->type(FL_HOR_SLIDER);
            s->bounds(ctrl.min, ctrl.max);
            s->step(ctrl.step);
            s->value(ctrl.default_val);
            w = s;
            break;
        }
        case CtrlType::Bool: {
            Fl_Check_Button* cb = new Fl_Check_Button(0, 0, 1, 1);
            cb->value(ctrl.default_val);
            w = cb;
            break;
        }
        case CtrlType::Btn: {
            w = new Fl_Button(0, 0, 1, 1, "Execute");
            break;
        }
        default:
            w = new Fl_Box(0, 0, 1, 1, "NOT SUPPORTED");
            break;
        }

        w->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        w->callback(MainWindow::_camCtrlCallback, cbData);
        
        grid->widget(lbl, i, 0);
        grid->widget(w, i++, 1);
    }

    grid->end();
    ctrls_wnd->end();
    
    ctrls_wnd->show();
}

void MainWindow::camCtrlCallback(uint32_t ctrl_id, int32_t val) {
    this->cam->setCtrl(ctrl_id, val);
}

// ---
void MainWindow::configAndStartCamStream() {
    if (!this->cam->config(this->_fmt_sel->value(), this->_res_sel->value())) {
        fl_alert("Failed to configure the camera.");
        return;
    }
    this->cam->startStream([this](FrameView fv) {
        if (this->should_cap.exchange(false)) {
            std::string filePath = this->settings.out_dir + "/" + Utils::getFormattedDt(settings.filename_fmt) + ".jpg";

            FILE *f = fopen(filePath.c_str(), "wb");
            if (f) {
                fwrite(fv.data, fv.size, 1, f);
                fclose(f);
            }

            Log::d() << "Saved to: " << filePath;
        }

        if (!this->frame_pending.exchange(true)) {
            {
                std::lock_guard<std::mutex> lock(this->frame_mtx);

                this->frame.resize(fv.size);
                this->frame.assign((byte*)fv.data, (byte*)fv.data + fv.size);
            }
            Fl::awake(MainWindow::_onFrameReceived, this);
        }
    });
}

// main
int main(int argc, char** argv) {
    MainWindow wnd {};
    wnd.show();
    return Fl::run();
}