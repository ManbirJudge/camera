#ifndef MAIN_H
#define MAIN_H

#include <atomic>
#include <memory>
#include <mutex>
#include <sstream>
#include <vector>

#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Flex.H>
#include <FL/Fl_Grid.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Value_Slider.H>
#include "stb_image.h"

#include "core.hpp"
#include "settings.hpp"
#include "utils.hpp"

#define UI_GAP 4

class MainWindow {
private:
    Settings settings{};

    std::vector<CamInfo> cameras;

    std::unique_ptr<Camera> cam{nullptr};

    std::mutex frame_mtx;
    std::vector<byte> frame;
    std::atomic<bool> frame_pending{false};

    std::atomic<bool> should_cap{false};

    Fl_Double_Window* wnd;
    Fl_Box* _canvas;
    Fl_Choice* _cam_sel;
    Fl_Choice* _fmt_sel;
    Fl_Choice* _res_sel;

    void initUi();

    static void _onWndClose(Fl_Widget* w, void* data);
    static void _onFrameReceived(void* data);
    static void _onCapBtnClick(Fl_Widget* w, void* data);
    static void _onCamSelChange(Fl_Widget* w, void* data);
    static void _onFmtSelChange(Fl_Widget* w, void* data);
    static void _onResSelChange(Fl_Widget* w, void* data);
    static void _onCtrlsBtnClick(Fl_Widget* w, void* data);
    static void _camCtrlCallback(Fl_Widget* w, void* data);

    void onWndClose();
    void onFrameReceived();
    void onCapBtnClick();
    void onCamSelChange();
    void onFmtSelChange();
    void onResSelChange();
    void onCtrlsBtnClick();
    void camCtrlCallback(uint32_t ctrl_id, int32_t val);

    void configAndStartCamStream();

public:
    MainWindow();
    
    void show();
};

typedef struct  {
    MainWindow* wnd;
    uint32_t ctrl_id;
} CtrlCallbackData;

#endif