#ifdef _WIN32
#include "utf8_to_utf16.h"
#include <windows.h>

bool utf8_to_utf16(const std::string &utf8, std::u16string &out)
{
    if (utf8.empty()) { out.clear(); return true; }
    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) return false;
    out.resize(needed);
    int rc = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), reinterpret_cast<wchar_t*>(&out[0]), needed);
    if (rc != needed) return false;
    return true;
}
#endif
