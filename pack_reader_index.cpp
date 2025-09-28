#include "pack_reader.h"

MiniPackIndex::MiniPackIndex() = default;

void MiniPackIndex::clear() {
    m_entries.clear();
    m_info_size = 0;
    m_data_start = 0;
}

size_t MiniPackIndex::file_count() const { return m_entries.size(); }

const std::vector<MiniPackEntry>& MiniPackIndex::entries() const { return m_entries; }

void MiniPackIndex::set_info_size(uint64_t s) { m_info_size = s; }
void MiniPackIndex::set_data_start(uint64_t s) { m_data_start = s; }

uint64_t MiniPackIndex::info_size() const { return m_info_size; }
uint64_t MiniPackIndex::data_start() const { return m_data_start; }
