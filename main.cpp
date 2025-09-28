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
#include "file_list_reader.h"

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
