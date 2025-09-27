#ifdef _WIN32
#include "encoding.h"
#include "utf8_to_utf16.h"
#include <windows.h>
#include <vector>
#include <fstream>
#include <iterator>

bool read_text_file_as_utf8(const std::string &path, std::string &out)
{
    // Read raw bytes
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    if (data.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        // UTF-8 BOM
        out.assign(reinterpret_cast<char*>(data.data() + 3), data.size() - 3);
        return true;
    }
    if (data.size() >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        // UTF-16 LE BOM
        // convert to UTF-8
        std::u16string u16;
        u16.resize((data.size() - 2) / 2);
        for (size_t i = 0; i < u16.size(); ++i) {
            u16[i] = static_cast<char16_t>(data[2 + i*2] | (data[2 + i*2 + 1] << 8));
        }
        // convert u16 to utf8 via WideCharToMultiByte using wchar on windows
        // first convert u16 to wide string
        std::wstring w;
        w.resize(u16.size());
        for (size_t i = 0; i < u16.size(); ++i) w[i] = static_cast<wchar_t>(u16[i]);
        int needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        if (needed <= 0) return false;
        out.resize(needed);
        int rc = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &out[0], needed, nullptr, nullptr);
        return rc == needed;
    }
    if (data.size() >= 2 && data[0] == 0xFE && data[1] == 0xFF) {
        // UTF-16 BE BOM
        std::u16string u16;
        u16.resize((data.size() - 2) / 2);
        for (size_t i = 0; i < u16.size(); ++i) {
            u16[i] = static_cast<char16_t>((data[2 + i*2] << 8) | data[2 + i*2 + 1]);
        }
        std::wstring w;
        w.resize(u16.size());
        for (size_t i = 0; i < u16.size(); ++i) w[i] = static_cast<wchar_t>(u16[i]);
        int needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        if (needed <= 0) return false;
        out.resize(needed);
        int rc = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &out[0], needed, nullptr, nullptr);
        return rc == needed;
    }

    // No BOM -> assume ANSI (CP_ACP)
    int needed = MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), nullptr, 0);
    if (needed <= 0) return false;
    std::wstring w;
    w.resize(needed);
    int rc = MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), &w[0], needed);
    if (rc <= 0) return false;
    int out_needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (out_needed <= 0) return false;
    out.resize(out_needed);
    rc = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &out[0], out_needed, nullptr, nullptr);
    return rc == out_needed;
}
#endif
