#include "mini_pack_builder_file.h"
#include "utf8_to_utf16.h"

#include <fstream>
#include <limits>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace {
constexpr std::size_t kCopyBufferSize = 64 * 1024;
}

bool add_file_to_builder(MiniPackBuilder &builder, const std::string &file_path, const std::string &stored_name_utf8, std::string &err)
{
    // Determine file size using ifstream to avoid dependency on std::filesystem
    std::ifstream in_size(file_path, std::ios::binary | std::ios::ate);
    if (!in_size) {
        err = "Failed to open input file to determine size: " + file_path;
        return false;
    }
    std::ifstream::pos_type pos = in_size.tellg();
    if (pos == static_cast<std::ifstream::pos_type>(-1)) {
        err = "Failed to determine size for file: " + file_path;
        return false;
    }
    std::uint64_t file_size = static_cast<std::uint64_t>(pos);
    in_size.close();

    if (file_size > std::numeric_limits<std::uint32_t>::max()) {
        err = "File too large (must fit in 32-bit size): " + file_path;
        return false;
    }
    auto size32 = static_cast<std::uint32_t>(file_size);
    std::string effective_name = stored_name_utf8.empty() ? file_path : stored_name_utf8;
    return builder.add_entry(effective_name, size32, [file_path, size32](const MiniPackBuilder::DataSink &sink, std::string &err_inner) {
        std::ifstream in(file_path, std::ios::binary);
        if (!in) {
            err_inner = "Failed to open input file: " + file_path;
            return false;
        }
        std::vector<char> buffer(kCopyBufferSize);
        std::uint64_t remaining = size32;
        while (remaining > 0) {
            std::size_t to_read = static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), remaining));
            in.read(buffer.data(), static_cast<std::streamsize>(to_read));
            std::streamsize got = in.gcount();
            if (got <= 0) {
                err_inner = "Failed to read from input file: " + file_path;
                return false;
            }
            if (!sink(reinterpret_cast<const std::uint8_t*>(buffer.data()), static_cast<std::size_t>(got))) {
                err_inner = "Failed to write file data to sink";
                return false;
            }
            remaining -= static_cast<std::uint64_t>(got);
        }
        return true;
    }, err);
}

bool add_file_to_builder(MiniPackBuilder &builder, const std::string &file_path, std::string &err)
{
    return add_file_to_builder(builder, file_path, file_path, err);
}
