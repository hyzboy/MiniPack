#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "pack_reader.h"

struct MiniPackBuildResult
{
    std::size_t info_size=0;
    std::uint64_t total_data_size=0;
    std::size_t file_count=0;
};

// Abstract writer interface
class MiniPackWriter
{
public:
    virtual ~MiniPackWriter()=default;
    // Write 'size' bytes from data, return true on success, false and set err on failure
    virtual bool write(const std::uint8_t *data,std::size_t size,std::string &err)=0;
};

// Factory functions to create default writers. Implementations hidden in cpp
std::unique_ptr<MiniPackWriter> create_vector_writer(std::vector<std::uint8_t> &out);
// Create a writer that writes directly to a file specified by path. Returns nullptr on failure to open file.
std::unique_ptr<MiniPackWriter> create_file_writer(const std::string &path);

class MiniPackBuilder
{
public:
    MiniPackBuilder();

    void clear();
    bool empty() const;
    std::size_t file_count() const;

    // Public API: add data buffers
    bool add_entry_from_buffer(const std::string &name_utf8,const std::vector<std::uint8_t> &data,std::string &err);

    // Build the pack and write into provided writer pointer.
    bool build_pack(MiniPackWriter *writer,bool index_only,MiniPackBuildResult &result,std::string &err) const;

protected:
    bool build_index(std::vector<std::uint8_t> &header,std::vector<std::uint32_t> &offsets,MiniPackBuildResult &result,std::string &err) const;

private:
    static void append_uint32(std::vector<std::uint8_t> &buf,std::uint32_t v);

    bool add_entry_internal(std::string name_utf8,std::vector<std::uint8_t> data,std::string &err);

    struct Entry
    {
        std::string name_utf8;
        std::vector<std::uint8_t> data;
    };

    std::vector<Entry> m_entries;
};
