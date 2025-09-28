#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Save a byte buffer to a file. Returns true on success, false and sets err on failure.
bool save_to_file(const std::string &path, const std::vector<std::uint8_t> &data, std::string &err);
