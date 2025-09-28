#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <algorithm>
#include <iterator>
#include <limits>

#include "encoding.h"
#include "utf8_to_utf16.h"
#include "mini_pack_builder.h"
#include "mini_pack_builder_file.h"

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

static bool read_file_list(const std::string &list_path, std::vector<std::string> &out_files, std::string &err)
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

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <list.txt> <output.pack> [--index-only|-i]\n";
        return 1;
    }

    std::string list_path = argv[1];
    std::string out_path = argv[2];
    bool index_only = false;
    if (argc >= 4) {
        std::string flag = argv[3];
        if (flag == "--index-only" || flag == "-i") index_only = true;
    }

    std::vector<std::string> files;
    std::string err;
    if (!read_file_list(list_path, files, err)) {
        std::cerr << err << "\n";
        return 1;
    }

    MiniPackBuilder builder;
    // Add files to builder (this will load files into memory)
    for (const auto &f : files) {
        if (!add_file_to_builder(builder, f, err)) {
            std::cerr << err << "\n";
            return 1;
        }
    }

    // Build pack by writing directly to output file using create_file_writer
    auto writer = create_file_writer(out_path);
    if (!writer) {
        std::cerr << "Failed to open output file: " << out_path << "\n";
        return 1;
    }

    MiniPackBuildResult result{};
    if (!builder.build_pack(writer.get(), index_only, result, err)) {
        std::cerr << err << "\n";
        return 1;
    }

    if (index_only)
        std::cout << "Wrote index (info block) for " << result.file_count << " files to " << out_path << " (info_size=" << result.info_size << " bytes, data=" << result.total_data_size << " bytes, data not written)\n";
    else
        std::cout << "Packed " << result.file_count << " files into " << out_path << " (info_size=" << result.info_size << " bytes, data=" << result.total_data_size << " bytes)\n";

    return 0;
}
