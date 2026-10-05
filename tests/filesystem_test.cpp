#include <iostream>
#include <filesystem>

#include "filesystem/file_enumerator.hpp"

int main() {
    std::filesystem::path test_directory = "test_data";

    std::cout << "UATC filesystem test started." << std::endl;
    std::cout << "Scanning: " << test_directory << std::endl;

    try {
        auto files = uatc::FileEnumerator::enumerate(test_directory);

        std::cout << "\nFiles found: " << files.size() << "\n";

        for (const auto& file : files) {
            std::cout
                << "Path: " << file.relative_path.generic_string()
                << " | Size: " << file.file_size
                << " bytes"
                << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}