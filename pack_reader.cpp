#include "pack_reader.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <system_error>

#include "utf8_to_utf16.h"
// Note: we don't have utf16_to_utf8 helper yet; implement a simple conversion using
// platform APIs or a fallback. For now provide a basic fallback that converts
// assuming ASCII subset (non-ASCII will be replaced).

static std::string utf16_to_utf8_fallback(const std::u16string &s)
{
    std::string out;
    out.reserve(s.size());
    for (char16_t c : s) {
        if (c <= 0x7F) out.push_back(static_cast<char>(c));
        else out.push_back('?');
    }
    return out;
}

MiniPackReader::MiniPackReader() = default;
MiniPackReader::~MiniPackReader() { close(); }

bool MiniPackReader::open(const std::string &path, std::string &err)
{
    close();
    m_path = path;
    m_in.open(path, std::ios::binary);
    if (!m_in) {
        err = "Failed to open pack file: " + path;
        return false;
    }

    // read magic
    char magic[8];
    m_in.read(magic, 8);
    if (m_in.gcount() != 8) {
        err = "Failed to read pack header";
        close();
        return false;
    }
    const char expect[8] = {'M','I','N','I','P','A','C','K'};
    if (std::memcmp(magic, expect, 8) != 0) {
        err = "Invalid pack magic";
        close();
        return false;
    }

    // read info size (uint32 little-endian)
    uint8_t b[4];
    m_in.read(reinterpret_cast<char*>(b), 4);
    if (m_in.gcount() != 4) {
        err = "Failed to read info size";
        close();
        return false;
    }
    uint32_t info_size = static_cast<uint32_t>(
        (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24));

    // read info block into memory
    std::vector<uint8_t> info;
    info.resize(info_size);
    m_in.read(reinterpret_cast<char*>(info.data()), static_cast<std::streamsize>(info_size));
    if (static_cast<size_t>(m_in.gcount()) != info_size) {
        err = "Failed to read info block";
        close();
        return false;
    }

    // parse info block
    size_t pos = 0;
    auto read_u32 = [&](uint32_t &out) -> bool {
        if (pos + 4 > info.size()) return false;
        out = static_cast<uint32_t>(info[pos] | (info[pos+1] << 8) | (info[pos+2] << 16) | (info[pos+3] << 24));
        pos += 4;
        return true;
    };
    uint32_t version = 0;
    if (!read_u32(version)) { err = "Info block corrupted (version)"; close(); return false; }
    uint32_t file_count = 0;
    if (!read_u32(file_count)) { err = "Info block corrupted (file_count)"; close(); return false; }

    m_entries.clear();
    m_entries.reserve(file_count);

    // read names
    for (uint32_t i = 0; i < file_count; ++i) {
        if (pos >= info.size()) { err = "Info block corrupted (name_len)"; close(); return false; }
        uint8_t name_len = info[pos++]; // count of char16_t
        if (pos + size_t(name_len) * 2 > info.size()) { err = "Info block corrupted (name bytes)"; close(); return false; }
        std::u16string name16;
        name16.reserve(name_len);
        for (uint8_t j = 0; j < name_len; ++j) {
            uint16_t v = static_cast<uint16_t>(info[pos] | (info[pos+1] << 8));
            pos += 2;
            name16.push_back(static_cast<char16_t>(v));
        }
        MiniPackEntry e;
        e.name16 = std::move(name16);
        e.name_utf8 = utf16_to_utf8_fallback(e.name16);
        m_entries.push_back(std::move(e));
    }

    // read metadata (size, offset) for each file
    for (uint32_t i = 0; i < file_count; ++i) {
        uint32_t sz=0, off=0;
        if (!read_u32(sz) || !read_u32(off)) { err = "Info block corrupted (metadata)"; close(); return false; }
        m_entries[i].size = sz;
        m_entries[i].offset = off;
    }

    // compute where data starts: 8 (magic) + 4 (info_size) + info_size
    m_info_size = info_size;
    m_data_start = 8 + 4 + m_info_size;

    return true;
}

void MiniPackReader::close()
{
    if (m_in.is_open()) m_in.close();
    m_path.clear();
    m_entries.clear();
    m_info_size = 0;
    m_data_start = 0;
}

bool MiniPackReader::is_open() const { return m_in.is_open(); }
size_t MiniPackReader::file_count() const { return m_entries.size(); }
const std::vector<MiniPackEntry>& MiniPackReader::entries() const { return m_entries; }

static std::string name_normalize_for_lookup(const std::string &s)
{
    // simple normalization: exact match only for now
    return s;
}

std::optional<std::vector<uint8_t>> MiniPackReader::read_file_data(const std::string &name_utf8, std::string &err) const
{
    std::string lookup = name_normalize_for_lookup(name_utf8);
    for (const auto &e : m_entries) {
        if (e.name_utf8 == lookup) {
            std::ifstream in(m_path, std::ios::binary);
            if (!in) { err = "Failed to open pack for reading: " + m_path; return std::nullopt; }
            in.seekg(static_cast<std::streamoff>(m_data_start + e.offset), std::ios::beg);
            std::vector<uint8_t> buf(e.size);
            in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(e.size));
            if (in.gcount() != static_cast<std::streamsize>(e.size)) { err = "Failed to read file data from pack"; return std::nullopt; }
            return buf;
        }
    }
    err = "File not found in pack: " + name_utf8;
    return std::nullopt;
}

bool MiniPackReader::read_file_to_stream(const std::string &name_utf8, std::ostream &out, std::string &err) const
{
    std::string lookup = name_normalize_for_lookup(name_utf8);
    for (const auto &e : m_entries) {
        if (e.name_utf8 == lookup) {
            std::ifstream in(m_path, std::ios::binary);
            if (!in) { err = "Failed to open pack for reading: " + m_path; return false; }
            in.seekg(static_cast<std::streamoff>(m_data_start + e.offset), std::ios::beg);
            std::vector<char> copybuf(64 * 1024);
            uint64_t remaining = e.size;
            while (remaining > 0) {
                size_t toread = static_cast<size_t>(std::min<uint64_t>(copybuf.size(), remaining));
                in.read(copybuf.data(), static_cast<std::streamsize>(toread));
                std::streamsize r = in.gcount();
                if (r <= 0) break;
                out.write(copybuf.data(), r);
                remaining -= static_cast<uint64_t>(r);
            }
            if (remaining != 0) { err = "Failed to read full file data from pack"; return false; }
            return true;
        }
    }
    err = "File not found in pack: " + name_utf8;
    return false;
}

bool MiniPackReader::extract_to_file(const std::string &name_utf8, const std::string &out_path, std::string &err) const
{
    std::ofstream out(out_path, std::ios::binary);
    if (!out) { err = "Failed to create output file: " + out_path; return false; }
    bool ok = read_file_to_stream(name_utf8, out, err);
    out.close();
    return ok;
}
