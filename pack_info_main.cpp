#include <iostream>
#include <iomanip>
#include <string>

#include "pack_reader_io.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <pack_file>\n";
        return 1;
    }

    const std::string pack_path = argv[1];
    MiniPackIndex index;
    std::string err;

    if (!load_minipack_index(pack_path, index, err)) {
        std::cerr << "Error: " << err << "\n";
        return 1;
    }

    const auto &entries = index.entries();
    const size_t count = index.file_count();

    std::cout << "Pack file : " << pack_path << "\n";
    std::cout << "Info size : " << index.info_size() << " bytes\n";
    std::cout << "Data start: " << index.data_start() << " bytes\n";
    std::cout << "File count: " << count << "\n";

    if (count == 0) {
        std::cout << "(no entries)\n";
        return 0;
    }

    std::cout << "\n";
    std::cout << std::left
              << std::setw(6)  << "Index"
              << std::setw(12) << "Offset"
              << std::setw(12) << "Size"
              << "Name"
              << "\n";
    std::cout << std::string(6 + 12 + 12 + 40, '-') << "\n";

    for (size_t i = 0; i < count; ++i) {
        const MiniPackEntry &e = entries[i];
        std::cout << std::left
                  << std::setw(6)  << i
                  << std::setw(12) << e.offset
                  << std::setw(12) << e.size
                  << e.name
                  << "\n";
    }

    return 0;
}
