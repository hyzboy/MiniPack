#include "mini_pack_builder.h"
#include <fstream>
#include <memory>

class FileWriterImpl : public MiniPackWriter {
public:
    explicit FileWriterImpl(const std::string &path) : m_out(path, std::ios::binary) {}
    bool ok() const { return static_cast<bool>(m_out); }
    bool write(const std::uint8_t *data, std::size_t size, std::string &err) override {
        if (size == 0) return true;
        m_out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        if (!m_out) {
            err = "Failed to write to file";
            return false;
        }
        return true;
    }
private:
    std::ofstream m_out;
};

std::unique_ptr<MiniPackWriter> create_file_writer(const std::string &path) {
    auto fw = std::unique_ptr<FileWriterImpl>(new FileWriterImpl(path));
    if (!fw->ok()) return nullptr;
    return std::unique_ptr<MiniPackWriter>(fw.release());
}
