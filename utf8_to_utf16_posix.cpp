#if !defined(_WIN32)
#include "utf8_to_utf16.h"
#include <iconv.h>
#include <cstring>

bool utf8_to_utf16(const std::string &utf8, std::u16string &out)
{
    if (utf8.empty()) { out.clear(); return true; }
    iconv_t cd = iconv_open("UTF-16LE", "UTF-8");
    if (cd == (iconv_t)-1) return false;

    size_t in_bytes_left = utf8.size();
    const char *in_buf = utf8.data();

    // Estimate output size: at most 4 bytes per input byte in worst cases, but for UTF-16 LE we need 2 bytes per code unit.
    std::vector<char> outbuf((in_bytes_left + 1) * 2);
    char *out_ptr = outbuf.data();
    size_t out_bytes_left = outbuf.size();

    // iconv expects char** and mutates pointers
    char *in_ptr = const_cast<char*>(in_buf);
    size_t res = iconv(cd, &in_ptr, &in_bytes_left, &out_ptr, &out_bytes_left);
    if (res == (size_t)-1) {
        iconv_close(cd);
        return false;
    }

    size_t out_written = outbuf.size() - out_bytes_left;
    // convert bytes to char16_t vector (little-endian)
    if (out_written % 2 != 0) { iconv_close(cd); return false; }
    size_t units = out_written / 2;
    out.clear();
    out.resize(units);
    const unsigned char *p = reinterpret_cast<const unsigned char*>(outbuf.data());
    for (size_t i = 0; i < units; ++i) {
        uint16_t v = static_cast<uint16_t>(p[i*2]) | (static_cast<uint16_t>(p[i*2+1]) << 8);
        out[i] = static_cast<char16_t>(v);
    }

    iconv_close(cd);
    return true;
}
#endif
