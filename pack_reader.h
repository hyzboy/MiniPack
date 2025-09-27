#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <fstream>
#include <ostream>

struct MiniPackEntry {
    std::u16string name16; // raw UTF-16 name from the info block
    std::string name_utf8; // converted UTF-8 name (if conversion succeeds)
    uint32_t size = 0;
    uint32_t offset = 0; // relative to start of data section
};

class MiniPackReader {
public:
    MiniPackReader();
    ~MiniPackReader();

    // Open a pack file. On success returns true and the reader can be queried.
    bool open(const std::string &path, std::string &err);

    // Close the currently opened pack (if any).
    void close();

    bool is_open() const;

    // Number of entries in the pack
    size_t file_count() const;

    // Get entries
    const std::vector<MiniPackEntry>& entries() const;

    // Extract a file by UTF-8 name to an output path
    bool extract_to_file(const std::string &name_utf8, const std::string &out_path, std::string &err) const;

    // Read file data into memory
    std::optional<std::vector<uint8_t>> read_file_data(const std::string &name_utf8, std::string &err) const;

    // Read file data into provided output stream
    bool read_file_to_stream(const std::string &name_utf8, std::ostream &out, std::string &err) const;

private:
    std::string m_path;
    std::ifstream m_in;
    uint64_t m_info_size = 0;
    uint64_t m_data_start = 0; // file offset where data section begins
    std::vector<MiniPackEntry> m_entries;

    static std::string utf16_to_utf8(const std::u16string &s);
};
