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
    bool add_entry_from_buffer(const std::string &name,const std::vector<std::uint8_t> &data,std::string &err);
    // Overload: add from raw pointer and size (uint32)
    bool add_entry_from_buffer(const std::string &name,const void *data,std::uint32_t size,std::string &err);

    template<typename T>
    bool add_entry_from_array(const std::string &name,const std::vector<T> &data,std::string &err)
    {
        return add_entry_from_buffer(name, reinterpret_cast<const std::uint8_t*>(data.data()), data.size()*sizeof(T), err);
    }

    // Build the pack and write into provided writer pointer.
    bool build_pack(MiniPackWriter *writer,bool index_only,MiniPackBuildResult &result,std::string &err) const;

protected:
    bool build_index(std::vector<std::uint8_t> &header,std::vector<std::uint32_t> &offsets,MiniPackBuildResult &result,std::string &err) const;

private:
    bool add_entry_internal(std::string name,std::vector<std::uint8_t> data,std::string &err);
    // Overload: internal add using raw pointer and size (uint32)
    bool add_entry_internal(std::string name,const void *data,std::uint32_t size,std::string &err);

    struct Entry
    {
        std::string name;
        std::vector<std::uint8_t> data;
    };

    std::vector<Entry> m_entries;
};

void write_string_list(MiniPackBuilder *builder,const std::string &entry_name,const std::vector<std::string> &list,std::string &err);

// ---- byte-stream buffer -------------------------------------------------
// Shared byte builder used both for variable-length entry serialization and
// larger aligned payload assembly.

struct ByteStreamBuffer
{
    std::vector<uint8_t> buf;

    uint32_t append_bytes(const void *data,uint32_t size,uint32_t align=1)
    {
        if(align>1)
        {
            const uint32_t mod=static_cast<uint32_t>(buf.size())%align;
            if(mod!=0)
                buf.insert(buf.end(),align-mod,0);
        }

        const uint32_t off=static_cast<uint32_t>(buf.size());

        if(size>0)
        {
            if(data)
            {
                const uint8_t *p=reinterpret_cast<const uint8_t *>(data);
                buf.insert(buf.end(),p,p+size);
            }
            else
            {
                buf.insert(buf.end(),size,0);
            }
        }

        return off;
    }

    template<typename T>
    uint32_t append_pod(const T &value,uint32_t align=1)
    {
        return append_bytes(&value,static_cast<uint32_t>(sizeof(T)),align);
    }

    uint32_t append(const void *data,uint32_t size,uint32_t align=4)
    {
        return append_bytes(data,size,align);
    }

    void push_i32(int32_t v)
    {
        append_pod(v);
    }

    void push_u32(uint32_t v)
    {
        append_pod(v);
    }

    // uint32 fileLen + char[fileLen]  (no NUL terminator)
    void push_str(const std::string &s)
    {
        push_u32(static_cast<uint32_t>(s.size()));
        append_bytes(s.empty()?nullptr:s.data(),static_cast<uint32_t>(s.size()));
    }

    uint32_t byte_size() const { return static_cast<uint32_t>(buf.size()); }
};
