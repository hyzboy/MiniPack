#pragma once

#include <string>

// Convert UTF-8 string to UTF-16 (std::u16string).
// Returns true on success, false on failure.
bool utf8_to_utf16(const std::string &utf8, std::u16string &out);
