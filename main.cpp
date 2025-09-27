#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <algorithm>
#include <iterator>
#include <limits>
#include <array>
#include <filesystem>

#include "encoding.h"
#include "utf8_to_utf16.h"

struct FileEntry
{
    std::string name_utf8; // used for file I/O and messages
    std::u16string name;   // UTF-16 name stored in info block
    uint32_t size = 0;
    uint32_t offset = 0; // offset relative to start of data section
};

static void append_uint16(std::vector<uint8_t> &buf, uint16_t v)
{
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}
static void append_uint32(std::vector<uint8_t> &buf, uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        buf.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
}
static void append_uint64(std::vector<uint8_t> &buf, uint64_t v)
{
    for (int i = 0; i < 8; ++i)
        buf.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
}

static std::string trim(const std::string &s)
{
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
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

    // Read list file as UTF-8 (detect BOM / encoding)
    std::string list_content_utf8;
    if (!read_text_file_as_utf8(list_path, list_content_utf8)) {
        std::cerr << "Failed to read or decode list file: " << list_path << "\n";
        return 1;
    }

    std::vector<FileEntry> files;
    std::string line;
    std::istringstream iss(list_content_utf8);
    while (std::getline(iss, line)) {
        std::string s = trim(line);
        if (s.empty()) continue;
        if (!s.empty() && s[0] == '#') continue; // comment
        FileEntry fe;
        fe.name_utf8 = s;
        if (!utf8_to_utf16(s, fe.name)) {
            std::cerr << "Failed to convert filename to UTF-16: " << s << "\n";
            return 1;
        }
        files.push_back(std::move(fe));
    }

    if (files.empty()) {
        std::cerr << "No files listed in " << list_path << "\n";
        return 1;
    }

    // Gather sizes for each file (determine file sizes before building info block)
    namespace fs = std::filesystem;
    for (auto &f : files) {
        std::error_code ec;
        // Use std::filesystem to get the file size instead of seeking
        uintmax_t sz = fs::file_size(f.name_utf8, ec);
        if (ec) {
            std::cerr << "Failed to get size for file: " << f.name_utf8 << " (" << ec.message() << ")\n";
            return 1;
        }
        if (sz > std::numeric_limits<uint32_t>::max()) {
            std::cerr << "File too large (must fit in 32-bit size): " << f.name_utf8 << "\n";
            return 1;
        }
        f.size = static_cast<uint32_t>(sz);
    }

    // Calculate total data size
    uint64_t total_data_size = 0;
    for (auto &f : files)
        total_data_size += f.size;

    if (total_data_size > std::numeric_limits<uint32_t>::max()) {
        std::cerr << "Total data too large for 32-bit offsets/sizes\n";
        return 1;
    }

    // Build info block in memory
    // Info format:
    // uint32_t version
    // uint32_t file_count
    // names block: for each file: uint8_t name_len (in char16_t units), name bytes (UTF-16 LE)
    // metadata block: for each file: uint32_t size, uint32_t offset (relative to data start), uint64_t xxh64, uint8_t sha1le[20]

    std::vector<uint8_t> info;
    append_uint32(info, 1); // version
    append_uint32(info, static_cast<uint32_t>(files.size()));

    // Names block (name length is one byte, count of char16_t units)
    for (auto &f : files) {
        if (f.name.size() > 0xFF) {
            std::string n = f.name_utf8.empty() ? "" : f.name_utf8;
            std::cerr << "Filename too long (max 255 UTF-16 units): " << n << "\n";
            return 1;
        }
        info.push_back(static_cast<uint8_t>(f.name.size()));
        // write each char16_t as little-endian two bytes
        for (char16_t ch : f.name) {
            uint16_t v = static_cast<uint16_t>(ch);
            info.push_back(static_cast<uint8_t>(v & 0xFF));
            info.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        }
    }

    // Calculate offsets relative to data start (which will be immediately after magic + info_size + info)
    uint64_t current_data_offset = 0; // relative to data start
    for (auto &f : files) {
        if (current_data_offset > std::numeric_limits<uint32_t>::max()) {
            std::cerr << "Package too large to fit 32-bit offsets\n";
            return 1;
        }
        f.offset = static_cast<uint32_t>(current_data_offset);
        current_data_offset += f.size;
    }

    // Metadata block: write size, offset, xxh64 and sha1le for each file
    for (auto &f : files) {
        append_uint32(info, f.size);
        append_uint32(info, f.offset);
    }

    // Now build final header: magic + uint32 info_size + info
    std::vector<uint8_t> header;
    const char magic[8] = { 'M','I','N','I','P','A','C','K' };
    header.insert(header.end(), magic, magic + 8);

    if (info.size() > std::numeric_limits<uint32_t>::max()) {
        std::cerr << "Info block too large\n";
        return 1;
    }
    append_uint32(header, static_cast<uint32_t>(info.size())); // info block size
    // append info
    header.insert(header.end(), info.begin(), info.end());

    // Write output file: header then concatenated data (unless index_only)
    std::ofstream out(out_path, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to create output file: " << out_path << "\n";
        return 1;
    }

    out.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));

    if (!index_only) {
        std::vector<char> copybuf(64 * 1024);
        for (const auto &f : files) {
            std::ifstream in(f.name_utf8, std::ios::binary);
            if (!in) {
                std::cerr << "Failed to reopen file: " << f.name_utf8 << "\n";
                return 1;
            }
            uint64_t remaining = f.size;
            while (remaining > 0) {
                size_t toread = static_cast<size_t>(std::min<uint64_t>(copybuf.size(), remaining));
                in.read(copybuf.data(), static_cast<std::streamsize>(toread));
                std::streamsize r = in.gcount();
                if (r <= 0) break;
                out.write(copybuf.data(), r);
                remaining -= static_cast<uint64_t>(r);
            }
        }
    }

    out.close();
    if (index_only)
        std::cout << "Wrote index (info block) for " << files.size() << " files to " << out_path << " (info_size=" << info.size() << " bytes, data=" << total_data_size << " bytes, data not written)\n";
    else
        std::cout << "Packed " << files.size() << " files into " << out_path << " (info_size=" << info.size() << " bytes, data=" << total_data_size << " bytes)\n";

    return 0;
}