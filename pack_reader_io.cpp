#include "pack_reader_io.h"
#include "pack_reader.h"
#include "utf_conv.h"

#include <fstream>
#include <cstring>

bool load_minipack_index(const std::string &path, MiniPackIndex &index, std::string &err)
{
    index.clear();
    std::ifstream in(path, std::ios::binary);
    if (!in) { err = "Failed to open pack file: " + path; return false; }

    char magic[8];
    in.read(magic, 8);
    if (in.gcount() != 8) { err = "Failed to read pack header"; return false; }
    const char expect[8] = {'M','I','N','I','P','A','C','K'};
    if (std::memcmp(magic, expect, 8) != 0) { err = "Invalid pack magic"; return false; }

    uint8_t b[4];
    in.read(reinterpret_cast<char*>(b), 4);
    if (in.gcount() != 4) { err = "Failed to read info size"; return false; }
    uint32_t info_size = static_cast<uint32_t>(b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24));

    std::vector<uint8_t> info(info_size);
    in.read(reinterpret_cast<char*>(info.data()), static_cast<std::streamsize>(info_size));
    if (static_cast<size_t>(in.gcount()) != info_size) { err = "Failed to read info block"; return false; }

    size_t pos = 0;
    auto read_u32 = [&](uint32_t &out) -> bool {
        if (pos + 4 > info.size()) return false;
        out = static_cast<uint32_t>(info[pos] | (info[pos+1] << 8) | (info[pos+2] << 16) | (info[pos+3] << 24));
        pos += 4;
        return true;
    };
    uint32_t version = 0;
    if (!read_u32(version)) { err = "Info block corrupted (version)"; return false; }
    uint32_t file_count = 0;
    if (!read_u32(file_count)) { err = "Info block corrupted (file_count)"; return false; }

    // Read name lengths block (file_count bytes)
    if (pos + file_count > info.size()) { err = "Info block corrupted (name lengths)"; return false; }
    std::vector<uint8_t> name_lengths(file_count);
    for (uint32_t i = 0; i < file_count; ++i) name_lengths[i] = info[pos++];

    index.m_entries.reserve(file_count);

    // Read all names as UTF-8, each followed by a NUL terminator
    for (uint32_t i = 0; i < file_count; ++i) {
        uint32_t len = name_lengths[i];
        if (pos + len + 1 > info.size()) { err = "Info block corrupted (names area)"; return false; }
        std::string name;
        if (len > 0) {
            name.assign(reinterpret_cast<const char*>(&info[pos]), len);
        }
        pos += len;
        if (info[pos++] != 0) { err = "Info block corrupted (missing NUL after name)"; return false; }

        MiniPackEntry e;
        e.name_utf8 = std::move(name);
        index.m_entries.push_back(std::move(e));
    }

    // Read per-file metadata
    for (uint32_t i = 0; i < file_count; ++i) {
        uint32_t sz=0, off=0;
        if (!read_u32(sz) || !read_u32(off)) { err = "Info block corrupted (metadata)"; return false; }
        index.m_entries[i].size = sz;
        index.m_entries[i].offset = off;
    }

    index.set_info_size(info_size);
    index.set_data_start(8 + 4 + info_size);
    return true;
}

bool read_minipack_entry_data(const std::string &path, const MiniPackEntry &entry, std::vector<uint8_t> &out, std::string &err)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) { err = "Failed to open pack for reading: " + path; return false; }
    // compute data start by reading header again
    // seek to info size
    in.seekg(8, std::ios::beg);
    uint8_t b[4];
    in.read(reinterpret_cast<char*>(b), 4);
    if (in.gcount() != 4) { err = "Failed to read info size"; return false; }
    uint32_t info_size = static_cast<uint32_t>(b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24));
    std::uint64_t data_start = 8 + 4 + info_size;
    in.seekg(static_cast<std::streamoff>(data_start + entry.offset), std::ios::beg);
    out.resize(entry.size);
    in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(entry.size));
    if (in.gcount() != static_cast<std::streamsize>(entry.size)) { err = "Failed to read file data from pack"; return false; }
    return true;
}

bool extract_minipack_entry_to_file(const std::string &path, const MiniPackEntry &entry, const std::string &out_path, std::string &err)
{
    std::vector<uint8_t> data;
    if (!read_minipack_entry_data(path, entry, data, err)) return false;
    std::ofstream out(out_path, std::ios::binary);
    if (!out) { err = "Failed to create output file: " + out_path; return false; }
    if (!data.empty()) out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return true;
}
