#include "settings.hpp"

Settings::Settings() {
    this->out_dir = Utils::getHomeDir() + DEFAULT_OUT_DIR_REL;

    std::ifstream f(Utils::getHomeDir() + SETTINGS_FILE_PATH_REL);
    std::string line;

    if (f.is_open()) {
        while (std::getline(f, line)) {
            if (line.empty()) continue;

            size_t delimiterPos = line.find('=');
            if (delimiterPos == std::string::npos) {
                Log::e() << "Delimiter not found in line: '" << line << '\'';
                continue;
            }
            
            std::string key = line.substr(0, delimiterPos);
            std::string val = line.substr(delimiterPos + 1);

            if (key == "out-dir")
                this->out_dir = val;
            else if (key == "filename-fmt")
                this->filename_fmt = val;
            else
                Log::w() << "Unknown settings key: '" << key << '\''; 
        }
        f.close();
    }

    if (!Utils::makeDir(this->out_dir)) {
        Log::w() << "Failed to create output directory. Setting it to CWD.";
        this->out_dir = ".";
    }
}

void Settings::save() {
    std::ofstream f(Utils::getHomeDir() + SETTINGS_FILE_PATH_REL);
    if (!f.is_open()) {
        Log::e() << "Failed to open settings file to write.";
        return;
    }

    f << "out-dir=" << this->out_dir << std::endl;
    f << "filename-fmt=" << this->filename_fmt << std::endl;

    f.close();
}