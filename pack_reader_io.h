#pragma once

#include "pack_reader.h"
#include <string>
#include <vector>

// Load an index from a pack file into MiniPackIndex. Returns true on success and sets err on failure.
bool load_minipack_index(const std::string &path, MiniPackIndex &index, std::string &err);

// Read file data by index entry into memory. Returns true on success and fills out buffer.
bool read_minipack_entry_data(const std::string &path, const MiniPackEntry &entry, std::vector<uint8_t> &out, std::string &err);

// Extract entry to file path
bool extract_minipack_entry_to_file(const std::string &path, const MiniPackEntry &entry, const std::string &out_path, std::string &err);
