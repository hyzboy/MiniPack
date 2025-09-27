#if !defined(_WIN32)
#include "encoding.h"
#include "utf8_to_utf16.h"
#include <iconv.h>
#include <vector>
#include <cstring>

bool read_text_file_as_utf8(const std::string &path, std::string &out)
{
    // Read raw bytes
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (data.size() >= 3 && static_cast<unsigned char>(data[0]) == 0xEF && static_cast<unsigned char>(data[1]) == 0xBB && static_cast<unsigned char>(data[2]) == 0xBF) {
        out.assign(data.data() + 3, data.size() - 3);
        return true;
    }
    if (data.size() >= 2 && (unsigned char)data[0] == 0xFF && (unsigned char)data[1] == 0xFE) {
        // UTF-16 LE
        // convert to UTF-8 using iconv
        iconv_t cd = iconv_open("UTF-8", "UTF-16LE");
        if (cd == (iconv_t)-1) return false;
        size_t in_bytes_left = data.size() - 2;
        char *in_ptr = data.data() + 2;
        std::vector<char> outbuf((in_bytes_left + 1) * 2);
        char *out_ptr = outbuf.data();
        size_t out_bytes_left = outbuf.size();
        size_t res = iconv(cd, &in_ptr, &in_bytes_left, &out_ptr, &out_bytes_left);
        iconv_close(cd);
        if (res == (size_t)-1) return false;
        out.assign(outbuf.data(), outbuf.size() - out_bytes_left);
        return true;
    }
    if (data.size() >= 2 && (unsigned char)data[0] == 0xFE && (unsigned char)data[1] == 0xFF) {
        // UTF-16 BE
        iconv_t cd = iconv_open("UTF-8", "UTF-16BE");
        if (cd == (iconv_t)-1) return false;
        size_t in_bytes_left = data.size() - 2;
        char *in_ptr = data.data() + 2;
        std::vector<char> outbuf((in_bytes_left + 1) * 2);
        char *out_ptr = outbuf.data();
        size_t out_bytes_left = outbuf.size();
        size_t res = iconv(cd, &in_ptr, &in_bytes_left, &out_ptr, &out_bytes_left);
        iconv_close(cd);
        if (res == (size_t)-1) return false;
        out.assign(outbuf.data(), outbuf.size() - out_bytes_left);
        return true;
    }

    // No BOM -> assume UTF-8 on POSIX
    out.assign(data.data(), data.size());
    return true;
}
#endif
