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
    using DataSink = std::function<bool(const std::uint8_t *data, std::size_t size)>;
    using DataWriter = std::function<bool(const DataSink &sink, std::string &err)>;

    MiniPackBuilder();

    void clear();
    bool empty() const;
    std::size_t file_count() const;

    bool add_entry(const std::string &name_utf8, std::uint32_t size, DataWriter writer, std::string &err);
    bool add_entry_from_buffer(const std::string &name_utf8, const void *data, std::size_t size, std::string &err);
    bool add_entry_from_buffer(const std::string &name_utf8, const std::vector<std::uint8_t> &data, std::string &err);

    // Build index/header only. Does not write file data.
    // 'header' receives the full pack header (magic + info block). 'offsets' receives the data offsets for each entry
    // (relative to the start of the data section). 'result' receives summary metrics.
    bool build_index(std::vector<std::uint8_t> &header, std::vector<std::uint32_t> &offsets, MiniPackBuildResult &result, std::string &err) const;

    // Build the complete pack in memory (header + concatenated data). If index_only is true, only header is produced.
    bool build_pack(std::vector<std::uint8_t> &out, bool index_only, MiniPackBuildResult &result, std::string &err) const;

    // Expose entries for external data writing.
    struct Entry {
        std::string name_utf8;
        std::u16string name_utf16;
        std::uint32_t size = 0;
        DataWriter writer;
    };

    const std::vector<Entry> &entries() const;

private:
    static void append_uint32(std::vector<std::uint8_t> &buf, std::uint32_t v);

    bool add_entry_internal(std::string name_utf8, std::u16string name_utf16, std::uint32_t size, DataWriter writer, std::string &err);

    std::vector<Entry> m_entries;
};
