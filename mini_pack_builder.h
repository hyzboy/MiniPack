#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct MiniPackBuildResult {
    std::size_t info_size = 0;
    std::uint64_t total_data_size = 0;
    std::size_t file_count = 0;
};

class MiniPackBuilder {
public:
    MiniPackBuilder();

    void clear();
    bool empty() const;
    std::size_t file_count() const;

    // Public API: add data buffers and build the pack
    bool add_entry_from_buffer(const std::string &name_utf8, const std::vector<std::uint8_t> &data, std::string &err);
    bool build_pack(std::vector<std::uint8_t> &out, bool index_only, MiniPackBuildResult &result, std::string &err) const;

protected:
    // Protected for potential subclassing: building index is an internal step
    bool build_index(std::vector<std::uint8_t> &header, std::vector<std::uint32_t> &offsets, MiniPackBuildResult &result, std::string &err) const;

private:
    static void append_uint32(std::vector<std::uint8_t> &buf, std::uint32_t v);

    bool add_entry_internal(std::string name_utf8, std::u16string name_utf16, std::vector<std::uint8_t> data, std::string &err);

    struct Entry {
        std::string name_utf8;
        std::u16string name_utf16;
        std::vector<std::uint8_t> data;
    };

    const std::vector<Entry> &entries() const { return m_entries; }

    std::vector<Entry> m_entries;
};
