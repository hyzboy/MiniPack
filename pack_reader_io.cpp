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

    // We need to support both old format (no encoding byte, names in UTF-8) and new format (per-name encoding byte).
    index.m_entries.reserve(file_count);
    for (uint32_t i = 0; i < file_count; ++i) {
        if (pos >= info.size()) { err = "Info block corrupted (name_len)"; return false; }
        // Peek first byte: if it's a known encoding value (0 or 1) then it's new format; otherwise treat as old-format name length (UTF-8)
        uint8_t first = info[pos];
        NameEncoding enc = NameEncoding::Utf8;
        size_t name_len = 0;
        if (first == static_cast<uint8_t>(NameEncoding::Utf8) || first == static_cast<uint8_t>(NameEncoding::Utf16Le)) {
            // new format: encoding byte followed by length
            enc = static_cast<NameEncoding>(first);
            pos++;
            if (pos >= info.size()) { err = "Info block corrupted (name_len after enc)"; return false; }
            name_len = info[pos++];
        } else {
            // old format: this byte was the length of UTF-8 name
            name_len = first;
            pos++;
            enc = NameEncoding::Utf8;
        }

        if (enc == NameEncoding::Utf8) {
            if (pos + name_len > info.size()) { err = "Info block corrupted (name bytes)"; return false; }
            std::string name;
            name.reserve(name_len);
            for (size_t j = 0; j < name_len; ++j) name.push_back(static_cast<char>(info[pos++]));

            MiniPackEntry e;
            e.name_utf8 = std::move(name);
            index.m_entries.push_back(std::move(e));
        } else if (enc == NameEncoding::Utf16Le) {
            // name_len is number of UTF-16 code units
            if (pos + name_len * 2 > info.size()) { err = "Info block corrupted (utf16 name bytes)"; return false; }
            std::u16string u16;
            u16.reserve(name_len);
            for (size_t j = 0; j < name_len; ++j) {
                uint16_t v = static_cast<uint16_t>(info[pos] | (info[pos+1] << 8));
                pos += 2;
                u16.push_back(static_cast<char16_t>(v));
            }
            // convert to utf8 - use helper
            std::string utf8;
            if (!utf16_string_to_utf8(u16, utf8)) {
                err = "Failed to convert UTF-16 name to UTF-8";
                return false;
            }
            MiniPackEntry e;
            e.name_utf8 = std::move(utf8);
            index.m_entries.push_back(std::move(e));
        } else {
            err = "Unsupported name encoding";
            return false;
        }
    }

    // read metadata
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
