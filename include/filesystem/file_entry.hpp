#ifndef UATC_FILE_ENTRY_HPP
#define UATC_FILE_ENTRY_HPP

#include <cstdint>
#include <filesystem>

namespace uatc {

struct FileEntry {
    std::filesystem::path relative_path;
    std::uint64_t file_size = 0;
};

} // namespace uatc

#endif // UATC_FILE_ENTRY_HPP