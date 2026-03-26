#ifndef SETTINGS_H
#define SETTINGS_H

#include <fstream>
#include <string>

#include "log.hpp"
#include "utils.hpp"

#define SETTINGS_FILE_PATH_REL "/.config/camera/config"

#define DEFAULT_OUT_DIR_REL "/Photos/Camera"
#define DEFAULT_FILENAME_FMT "%d-%m-%Y %H:%M:%S"
#define DEFAULT_SOUND_EFFECTS true

class Settings {
public:
    std::string out_dir;
    std::string filename_fmt{DEFAULT_FILENAME_FMT};
    bool sound_effects{DEFAULT_SOUND_EFFECTS};

    Settings();

    void save();
};

#endif