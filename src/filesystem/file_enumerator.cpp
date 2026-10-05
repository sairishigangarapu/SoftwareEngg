#include "filesystem/file_enumerator.hpp"

#include <algorithm>
#include <stdexcept>

namespace uatc {

std::vector<FileEntry> FileEnumerator::enumerate(
    const std::filesystem::path& root
) {
    if (!std::filesystem::exists(root)) {
        throw std::runtime_error("Input path does not exist.");
    }

    if (!std::filesystem::is_directory(root)) {
        throw std::runtime_error("Input path is not a directory.");
    }

    std::vector<FileEntry> files;

    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root)) {

        if (!entry.is_regular_file()) {
            continue;
        }

        FileEntry file;

        file.relative_path =
            std::filesystem::relative(entry.path(), root);

        file.file_size =
            std::filesystem::file_size(entry.path());

        files.push_back(file);
    }

    std::sort(
        files.begin(),
        files.end(),
        [](const FileEntry& a, const FileEntry& b) {
            return a.relative_path.generic_string()
                 < b.relative_path.generic_string();
        }
    );

    return files;
}

} // namespace uatc