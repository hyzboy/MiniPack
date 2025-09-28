#include "file_list_reader.h"

#include <sstream>
#include <algorithm>

#include "encoding.h"

static std::string trim(const std::string &s)
{
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Remove UTF-8 BOM (0xEF,0xBB,0xBF) or Unicode BOM character at start of the string
static void remove_utf8_bom(std::string &s)
{
    if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF && static_cast<unsigned char>(s[1]) == 0xBB && static_cast<unsigned char>(s[2]) == 0xBF) {
        s.erase(0, 3);
        return;
    }
    // Also handle a single U+FEFF encoded in UTF-8 (same bytes as above) just in case
    if (!s.empty() && static_cast<unsigned char>(s[0]) == 0xEF) {
        // try to remove any initial U+FEFF byte sequence if present
        if (s.size() >= 3 && static_cast<unsigned char>(s[1]) == 0xBB && static_cast<unsigned char>(s[2]) == 0xBF) {
            s.erase(0, 3);
            return;
        }
    }
}

bool read_file_list(const std::string &list_path, std::vector<std::string> &out_files, std::string &err)
{
    std::string list_content_utf8;
    if (!read_text_file_as_utf8(list_path, list_content_utf8)) {
        err = "Failed to read or decode list file: " + list_path;
        return false;
    }

    // Ensure any BOM at the start of the file is removed
    remove_utf8_bom(list_content_utf8);

    std::istringstream iss(list_content_utf8);
    std::string line;
    while (std::getline(iss, line)) {
        // Remove BOM from individual lines as well (some editors/files may include BOM on first line only)
        remove_utf8_bom(line);
        std::string s = trim(line);
        if (s.empty()) continue;
        if (!s.empty() && s[0] == '#') continue; // comment
        out_files.push_back(s);
    }

    if (out_files.empty()) {
        err = "No files listed in " + list_path;
        return false;
    }
    return true;
}
