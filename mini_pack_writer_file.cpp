#include "mini_pack_builder.h"
#include <fstream>
#include <memory>
#include <iostream>
#include <string>

class FileWriterImpl : public MiniPackWriter {
public:
    explicit FileWriterImpl(const std::string &path)
        : m_out(path, std::ios::binary | std::ios::out | std::ios::trunc), m_path(path)
    {
        if (m_out.is_open() && static_cast<bool>(m_out)) {
            std::cout << "[MiniPack][FileWriter] " << m_path << " - Opened output file" << "\n";
        } else {
            std::cerr << "[MiniPack][FileWriter] " << m_path << " - Failed to open output file" << "\n";
        }
    }

    ~FileWriterImpl() override {
        if (m_out.is_open()) {
            m_out.flush();
            if (!m_out) {
                std::cerr << "[MiniPack][FileWriter] " << m_path << " - Flush failed" << "\n";
            } else {
                std::cout << "[MiniPack][FileWriter] " << m_path << " - Flushed file successfully" << "\n";
            }
            m_out.close();
            std::cout << "[MiniPack][FileWriter] " << m_path << " - Closed output file" << "\n";
        }
    }

    bool ok() const { return m_out.is_open() && static_cast<bool>(m_out); }

    bool write(const std::uint8_t *data, std::size_t size, std::string &err) override {
        if (size == 0) {
            std::cout << "[MiniPack][FileWriter] " << m_path << " - Skipping zero-size write" << "\n";
            return true;
        }
        std::cout << "[MiniPack][FileWriter] " << m_path << " - Writing " << size << " bytes" << "\n";
        m_out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        if (!m_out) {
            err = "Failed to write to file";
            std::cerr << "[MiniPack][FileWriter] " << m_path << " - Write failed (" << size << " bytes)" << "\n";
            return false;
        }
        std::cout << "[MiniPack][FileWriter] " << m_path << " - Wrote " << size << " bytes" << "\n";
        return true;
    }

private:
    std::ofstream m_out;
    std::string m_path;
};

std::unique_ptr<MiniPackWriter> create_file_writer(const std::string &path) {
    std::cout << "[MiniPack][FileWriter] " << path << " - Creating file writer" << "\n";
    auto fw = std::make_unique<FileWriterImpl>(path);
    if (!fw->ok()) {
        std::cerr << "[MiniPack][FileWriter] " << path << " - File writer creation failed" << "\n";
        return nullptr;
    }
    std::cout << "[MiniPack][FileWriter] " << path << " - File writer created" << "\n";
    return fw;
}
