#include "utf16_to_utf8.h"
#include <vector>

static inline uint16_t read_u16(const uint8_t* p, bool big_endian) {
    if (big_endian) return static_cast<uint16_t>((p[0] << 8) | p[1]);
    return static_cast<uint16_t>((p[1] << 8) | p[0]);
}

static void append_utf8_from_codepoint(std::string &out, uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

bool utf16_bytes_to_utf8(const uint8_t* data, size_t size, bool big_endian, std::string &out) {
    if (size % 2 != 0) return false;
    out.clear();
    size_t i = 0;
    while (i + 1 < size) {
        uint16_t w1 = read_u16(data + i, big_endian);
        i += 2;
        if (w1 >= 0xD800 && w1 <= 0xDBFF) {
            // high surrogate, need low surrogate next
            if (i + 1 >= size) return false;
            uint16_t w2 = read_u16(data + i, big_endian);
            i += 2;
            if (w2 < 0xDC00 || w2 > 0xDFFF) return false;
            uint32_t cp = 0x10000 + (((w1 - 0xD800) << 10) | (w2 - 0xDC00));
            append_utf8_from_codepoint(out, cp);
        } else if (w1 >= 0xDC00 && w1 <= 0xDFFF) {
            // unpaired low surrogate
            return false;
        } else {
            append_utf8_from_codepoint(out, w1);
        }
    }
    return true;
}
