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
#include "utf_conv.h"
#include "mini_pack_builder.h"
#include "mini_pack_builder_file.h"
#include "file_list_reader.h"
#include "dir_scan.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <list.txt|directory> <output.pack> [--index-only|-i]\n";
        return 1;
    }

    std::string list_path = argv[1];
    std::string out_path = argv[2];
    bool index_only = false;
    if (argc >= 4) {
        std::string flag = argv[3];
        if (flag == "--index-only" || flag == "-i") index_only = true;
    }

    // Use a vector of pairs: {disk_path, stored_name_in_pack}
    std::vector<std::pair<std::string, std::string>> file_pairs;
    std::string err;

    // If the input path is a directory, enumerate files recursively and store
    // them with relative paths as their stored names inside the pack.
    if (!collect_files_from_directory(list_path, file_pairs, err)) {
        // If collection failed, fall back to treating list_path as a file list
        // (collect_files_from_directory sets err on failure; but read_file_list may be the real intention)
        file_pairs.clear();
        std::vector<std::string> files;
        if (!read_file_list(list_path, files, err)) {
            std::cerr << err << "\n";
            return 1;
        }
        for (const auto &f : files) file_pairs.emplace_back(f, f);
    }

    MiniPackBuilder builder;
    // Add files to builder (this will load files into memory)
    for (const auto &p : file_pairs) {
        if (!add_file_to_builder(builder, p.first, p.second, err)) {
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
