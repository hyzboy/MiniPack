#include "save_to_file.h"
#include <fstream>

bool save_to_file(const std::string &path, const std::vector<std::uint8_t> &data, std::string &err)
{
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        err = "Failed to create output file: " + path;
        return false;
    }
    if (!data.empty()) {
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!out) {
            err = "Failed to write data to file: " + path;
            return false;
        }
    }
    out.close();
    return true;
}
