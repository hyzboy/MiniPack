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

// Read and parse list file into FileEntry vector
static bool read_file_list(const std::string &list_path, std::vector<FileEntry> &out_files, std::string &err)
{
    std::string list_content_utf8;
    if (!read_text_file_as_utf8(list_path, list_content_utf8)) {
        err = "Failed to read or decode list file: " + list_path;
        return false;
    }

    std::istringstream iss(list_content_utf8);
    std::string line;
    while (std::getline(iss, line)) {
        std::string s = trim(line);
        if (s.empty()) continue;
        if (!s.empty() && s[0] == '#') continue; // comment
        FileEntry fe;
        fe.name_utf8 = s;
        if (!utf8_to_utf16(s, fe.name)) {
            err = "Failed to convert filename to UTF-16: " + s;
            return false;
        }
        out_files.push_back(std::move(fe));
    }

    if (out_files.empty()) {
        err = "No files listed in " + list_path;
        return false;
    }
    return true;
}

// Gather file sizes using std::filesystem
static bool gather_file_sizes(std::vector<FileEntry> &files, std::string &err)
{
    namespace fs = std::filesystem;
    for (auto &f : files) {
        std::error_code ec;
        uintmax_t sz = fs::file_size(f.name_utf8, ec);
        if (ec) {
            err = "Failed to get size for file: " + f.name_utf8 + " (" + ec.message() + ")";
            return false;
        }
        if (sz > std::numeric_limits<uint32_t>::max()) {
            err = "File too large (must fit in 32-bit size): " + f.name_utf8;
            return false;
        }
        f.size = static_cast<uint32_t>(sz);
    }
    return true;
}

// Build the info block and compute offsets
static bool build_info_block(std::vector<FileEntry> &files, std::vector<uint8_t> &info, std::string &err, uint64_t &out_total_data_size)
{
    out_total_data_size = 0;
    for (auto &f : files)
        out_total_data_size += f.size;

    if (out_total_data_size > std::numeric_limits<uint32_t>::max()) {
        err = "Total data too large for 32-bit offsets/sizes";
        return false;
    }

    append_uint32(info, 1); // version
    append_uint32(info, static_cast<uint32_t>(files.size()));

    // Names block
    for (auto &f : files) {
        if (f.name.size() > 0xFF) {
            std::string n = f.name_utf8.empty() ? "" : f.name_utf8;
            err = "Filename too long (max 255 UTF-16 units): " + n;
            return false;
        }
        info.push_back(static_cast<uint8_t>(f.name.size()));
        for (char16_t ch : f.name) {
            uint16_t v = static_cast<uint16_t>(ch);
            info.push_back(static_cast<uint8_t>(v & 0xFF));
            info.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        }
    }

    // Calculate offsets relative to data start
    uint64_t current_data_offset = 0;
    for (auto &f : files) {
        if (current_data_offset > std::numeric_limits<uint32_t>::max()) {
            err = "Package too large to fit 32-bit offsets";
            return false;
        }
        f.offset = static_cast<uint32_t>(current_data_offset);
        current_data_offset += f.size;
    }

    // Metadata block (size, offset)
    for (auto &f : files) {
        append_uint32(info, f.size);
        append_uint32(info, f.offset);
    }

    return true;
}

// Write header and optionally file data
static bool write_output_file(const std::string &out_path, const std::vector<uint8_t> &header, const std::vector<FileEntry> &files, bool index_only, uint64_t total_data_size, std::string &err)
{
    std::ofstream out(out_path, std::ios::binary);
    if (!out) {
        err = "Failed to create output file: " + out_path;
        return false;
    }

    out.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));

    if (!index_only) {
        std::vector<char> copybuf(64 * 1024);
        for (const auto &f : files) {
            std::ifstream in(f.name_utf8, std::ios::binary);
            if (!in) {
                err = "Failed to reopen file: " + f.name_utf8;
                return false;
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

    std::vector<FileEntry> files;
    std::string err;
    if (!read_file_list(list_path, files, err)) {
        std::cerr << err << "\n";
        return 1;
    }

    if (!gather_file_sizes(files, err)) {
        std::cerr << err << "\n";
        return 1;
    }

    std::vector<uint8_t> info;
    uint64_t total_data_size = 0;
    if (!build_info_block(files, info, err, total_data_size)) {
        std::cerr << err << "\n";
        return 1;
    }

    // Build final header: magic + uint32 info_size + info
    std::vector<uint8_t> header;
    const char magic[8] = { 'M','I','N','I','P','A','C','K' };
    header.insert(header.end(), magic, magic + 8);

    if (info.size() > std::numeric_limits<uint32_t>::max()) {
        std::cerr << "Info block too large\n";
        return 1;
    }
    append_uint32(header, static_cast<uint32_t>(info.size())); // info block size
    header.insert(header.end(), info.begin(), info.end());

    if (!write_output_file(out_path, header, files, index_only, total_data_size, err)) {
        std::cerr << err << "\n";
        return 1;
    }

    if (index_only)
        std::cout << "Wrote index (info block) for " << files.size() << " files to " << out_path << " (info_size=" << info.size() << " bytes, data=" << total_data_size << " bytes, data not written)\n";
    else
        std::cout << "Packed " << files.size() << " files into " << out_path << " (info_size=" << info.size() << " bytes, data=" << total_data_size << " bytes)\n";

    return 0;
}