#include "mini_pack_builder.h"

#include "utf8_to_utf16.h"

#include <limits>
#include <vector>
#include <memory>

namespace {
const char kMagic[8] = { 'M','I','N','I','P','A','C','K' };
}

MiniPackBuilder::MiniPackBuilder() = default;

void MiniPackBuilder::clear() { m_entries.clear(); }

bool MiniPackBuilder::empty() const { return m_entries.empty(); }

std::size_t MiniPackBuilder::file_count() const { return m_entries.size(); }

bool MiniPackBuilder::add_entry_from_buffer(const std::string &name_utf8, const std::vector<std::uint8_t> &data, std::string &err)
{
    std::u16string name_utf16;
    if (!utf8_to_utf16(name_utf8, name_utf16)) {
        err = "Failed to convert name to UTF-16: " + name_utf8;
        return false;
    }
    if (data.size() > std::numeric_limits<std::uint32_t>::max()) {
        err = "Buffer too large for MiniPack entry";
        return false;
    }
    return add_entry_internal(name_utf8, std::move(name_utf16), data, err);
}

bool MiniPackBuilder::build_index(std::vector<std::uint8_t> &header, std::vector<std::uint32_t> &offsets, MiniPackBuildResult &result, std::string &err) const
{
    if (m_entries.empty()) {
        err = "No entries added to MiniPack";
        return false;
    }

    std::vector<std::uint8_t> info;
    info.reserve(16 * m_entries.size());

    append_uint32(info, 1); // version
    append_uint32(info, static_cast<std::uint32_t>(m_entries.size()));

    for (const auto &entry : m_entries) {
        if (entry.name_utf16.size() > 0xFF) {
            err = "Filename too long (max 255 UTF-16 units): " + entry.name_utf8;
            return false;
        }
        info.push_back(static_cast<std::uint8_t>(entry.name_utf16.size()));
        for (char16_t ch : entry.name_utf16) {
            std::uint16_t v = static_cast<std::uint16_t>(ch);
            info.push_back(static_cast<std::uint8_t>(v & 0xFF));
            info.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        }
    }

    offsets.clear();
    offsets.reserve(m_entries.size());
    std::uint64_t current_offset = 0;
    std::uint64_t total_data_size = 0;

    for (const auto &entry : m_entries) {
        total_data_size += entry.data.size();
        if (total_data_size > std::numeric_limits<std::uint32_t>::max()) {
            err = "Total data too large for 32-bit offsets/sizes";
            return false;
        }
    }

    for (const auto &entry : m_entries) {
        offsets.push_back(static_cast<std::uint32_t>(current_offset));
        current_offset += entry.data.size();
    }

    for (std::size_t i = 0; i < m_entries.size(); ++i) {
        append_uint32(info, static_cast<std::uint32_t>(m_entries[i].data.size()));
        append_uint32(info, offsets[i]);
    }

    if (info.size() > std::numeric_limits<std::uint32_t>::max()) {
        err = "Info block too large";
        return false;
    }

    header.clear();
    header.insert(header.end(), std::begin(kMagic), std::end(kMagic));
    append_uint32(header, static_cast<std::uint32_t>(info.size()));
    header.insert(header.end(), info.begin(), info.end());

    result.info_size = info.size();
    result.total_data_size = total_data_size;
    result.file_count = m_entries.size();
    return true;
}

bool MiniPackBuilder::build_pack(MiniPackWriter *writer, bool index_only, MiniPackBuildResult &result, std::string &err) const
{
    if (!writer) { err = "Writer is null"; return false; }

    std::vector<std::uint8_t> header;
    std::vector<std::uint32_t> offsets;
    if (!build_index(header, offsets, result, err)) return false;

    if (!writer->write(header.data(), header.size(), err)) return false;
    if (index_only) return true;

    for (const auto &entry : m_entries) {
        if (!entry.data.empty()) {
            if (!writer->write(entry.data.data(), entry.data.size(), err)) return false;
        }
    }

    return true;
}

void MiniPackBuilder::append_uint32(std::vector<std::uint8_t> &buf, std::uint32_t v)
{
    for (int i = 0; i < 4; ++i) buf.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
}

bool MiniPackBuilder::add_entry_internal(std::string name_utf8, std::u16string name_utf16, std::vector<std::uint8_t> data, std::string &err)
{
    if (name_utf8.empty()) { err = "Entry name cannot be empty"; return false; }
    if (data.size() > std::numeric_limits<std::uint32_t>::max()) { err = "Entry size exceeds limit"; return false; }
    m_entries.push_back(Entry{std::move(name_utf8), std::move(name_utf16), std::move(data)});
    return true;
}
