#ifndef UATC_FILE_ENUMERATOR_HPP
#define UATC_FILE_ENUMERATOR_HPP

#include <filesystem>
#include <vector>

#include "file_entry.hpp"

namespace uatc {

class FileEnumerator {
public:
    static std::vector<FileEntry> enumerate(
        const std::filesystem::path& root
    );
};

} // namespace uatc

#endif // UATC_FILE_ENUMERATOR_HPP