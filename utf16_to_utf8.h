#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Convert raw UTF-16 bytes (without BOM) to UTF-8 string.
// `data` points to the bytes, `size` is the byte count (should be even).
// `big_endian` indicates whether the input is UTF-16BE (true) or UTF-16LE (false).
// Returns true on success, false on invalid input (e.g., odd size or invalid surrogates).
bool utf16_bytes_to_utf8(const uint8_t* data, size_t size, bool big_endian, std::string &out);
