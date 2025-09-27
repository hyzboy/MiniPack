#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <algorithm>
#include <iterator>
#include <limits>

struct FileEntry
{
    std::string name;
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

    // Read list file
    std::ifstream list_in(list_path);
    if (!list_in) {
        std::cerr << "Failed to open list file: " << list_path << "\n";
        return 1;
    }

    std::vector<FileEntry> files;
    std::string line;
    while (std::getline(list_in, line)) {
        std::string s = trim(line);
        if (s.empty()) continue;
        if (!s.empty() && s[0] == '#') continue; // comment
        files.push_back(FileEntry{s, 0, 0});
    }

    if (files.empty()) {
        std::cerr << "No files listed in " << list_path << "\n";
        return 1;
    }

    // Gather sizes
    uint64_t total_data_size = 0;
    for (auto &f : files) {
        std::ifstream in(f.name, std::ios::binary | std::ios::ate);
        if (!in) {
            std::cerr << "Failed to open file: " << f.name << "\n";
            return 1;
        }
        std::ifstream::pos_type pos = in.tellg();
        if (pos < 0) pos = 0;
        uint64_t sz = static_cast<uint64_t>(pos);
        if (sz > std::numeric_limits<uint32_t>::max()) {
            std::cerr << "File too large for 32-bit size: " << f.name << "\n";
            return 1;
        }
        f.size = static_cast<uint32_t>(sz);
        total_data_size += sz;
    }

    if (total_data_size > std::numeric_limits<uint32_t>::max()) {
        std::cerr << "Total data too large for 32-bit offsets/sizes\n";
        return 1;
    }

    // Build info block in memory
    // Info format:
    // uint32_t version
    // uint32_t file_count
    // names block: for each file: uint8_t name_len, name bytes
    // metadata block: for each file: uint32_t size, uint32_t offset (relative to data start)

    std::vector<uint8_t> info;
    append_uint32(info, 1); // version
    append_uint32(info, static_cast<uint32_t>(files.size()));

    // Names block (name length is one byte)
    for (auto &f : files) {
        if (f.name.size() > 0xFF) {
            std::cerr << "Filename too long (max 255): " << f.name << "\n";
            return 1;
        }
        info.push_back(static_cast<uint8_t>(f.name.size()));
        info.insert(info.end(), f.name.begin(), f.name.end());
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

    // Metadata block: write size and offset for each file (32-bit)
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
        const size_t bufsize = 64 * 1024;
        std::vector<char> buffer(bufsize);

        for (const auto &f : files) {
            std::ifstream in(f.name, std::ios::binary);
            if (!in) {
                std::cerr << "Failed to reopen file: " << f.name << "\n";
                return 1;
            }
            uint64_t remaining = f.size;
            while (remaining > 0) {
                size_t toread = static_cast<size_t>(std::min<uint64_t>(bufsize, remaining));
                in.read(buffer.data(), static_cast<std::streamsize>(toread));
                std::streamsize r = in.gcount();
                if (r <= 0) break;
                out.write(buffer.data(), r);
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