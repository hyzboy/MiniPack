#include "mini_pack_builder.h"

#include "utf8_to_utf16.h"

#include <limits>
#include <memory>
#include <vector>

namespace {
const char kMagic[8] = { 'M','I','N','I','P','A','C','K' };
}

MiniPackBuilder::MiniPackBuilder() = default;

void MiniPackBuilder::clear() { m_entries.clear(); }

bool MiniPackBuilder::empty() const { return m_entries.empty(); }

std::size_t MiniPackBuilder::file_count() const { return m_entries.size(); }

bool MiniPackBuilder::add_entry(const std::string &name_utf8, std::uint32_t size, DataWriter writer, std::string &err)
{
    std::u16string name_utf16;
    if (!utf8_to_utf16(name_utf8, name_utf16)) {
        err = "Failed to convert name to UTF-16: " + name_utf8;
        return false;
    }
    return add_entry_internal(name_utf8, std::move(name_utf16), size, std::move(writer), err);
}

bool MiniPackBuilder::add_entry_from_buffer(const std::string &name_utf8, const void *data, std::size_t size, std::string &err)
{
    if (!data && size != 0) {
        err = "Null data pointer provided";
        return false;
    }
    if (size > std::numeric_limits<std::uint32_t>::max()) {
        err = "Buffer too large for MiniPack entry";
        return false;
    }
    auto buffer = std::make_shared<std::vector<std::uint8_t>>(static_cast<const std::uint8_t*>(data), static_cast<const std::uint8_t*>(data) + size);
    return add_entry(name_utf8, static_cast<std::uint32_t>(size), [buffer](const DataSink &sink, std::string &err_inner) {
        if (!buffer->empty() && !sink(buffer->data(), buffer->size())) {
            err_inner = "Failed to write entry data";
            return false;
        }
        return true;
    }, err);
}

bool MiniPackBuilder::add_entry_from_buffer(const std::string &name_utf8, const std::vector<std::uint8_t> &data, std::string &err)
{
    return add_entry_from_buffer(name_utf8, data.data(), data.size(), err);
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
        total_data_size += entry.size;
        if (total_data_size > std::numeric_limits<std::uint32_t>::max()) {
            err = "Total data too large for 32-bit offsets/sizes";
            return false;
        }
    }

    for (const auto &entry : m_entries) {
        offsets.push_back(static_cast<std::uint32_t>(current_offset));
        current_offset += entry.size;
    }

    for (std::size_t i = 0; i < m_entries.size(); ++i) {
        append_uint32(info, m_entries[i].size);
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

bool MiniPackBuilder::build_pack(std::vector<std::uint8_t> &out, bool index_only, MiniPackBuildResult &result, std::string &err) const
{
    // Build index first
    std::vector<std::uint8_t> header;
    std::vector<std::uint32_t> offsets;
    if (!build_index(header, offsets, result, err)) return false;

    out = header;
    if (index_only) return true;

    // Reserve space for data section to avoid repeated reallocations
    if (result.total_data_size <= std::numeric_limits<std::size_t>::max()) {
        try {
            out.reserve(out.size() + static_cast<std::size_t>(result.total_data_size));
        } catch (...) {
            // ignore reservation failure, will still work
        }
    }

    // For each entry, invoke writer with a sink that appends to 'out'
    for (const auto &entry : m_entries) {
        std::uint64_t written = 0;
        auto sink = [&out, &written](const std::uint8_t *data, std::size_t chunk) -> bool {
            if (chunk == 0) return true;
            size_t old = out.size();
            out.insert(out.end(), data, data + chunk);
            written += chunk;
            return true;
        };

        if (!entry.writer(sink, err)) {
            if (err.empty()) err = "Failed to write data for entry: " + entry.name_utf8;
            return false;
        }
        if (written != entry.size) {
            err = "Data size mismatch for entry: " + entry.name_utf8;
            return false;
        }
    }

    return true;
}

const std::vector<MiniPackBuilder::Entry> &MiniPackBuilder::entries() const
{
    return m_entries;
}

void MiniPackBuilder::append_uint32(std::vector<std::uint8_t> &buf, std::uint32_t v)
{
    for (int i = 0; i < 4; ++i) {
        buf.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
    }
}

bool MiniPackBuilder::add_entry_internal(std::string name_utf8, std::u16string name_utf16, std::uint32_t size, DataWriter writer, std::string &err)
{
    if (name_utf8.empty()) {
        err = "Entry name cannot be empty";
        return false;
    }
    if (size > std::numeric_limits<std::uint32_t>::max()) {
        err = "Entry size exceeds limit";
        return false;
    }
    if (!writer) {
        err = "Data writer callback not provided";
        return false;
    }
    m_entries.push_back(Entry{std::move(name_utf8), std::move(name_utf16), size, std::move(writer)});
    return true;
}
