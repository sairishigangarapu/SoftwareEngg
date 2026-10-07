#include <iostream>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "filesystem/file_enumerator.hpp"
#include "filesystem/file_reader.hpp"

int main() {
    std::filesystem::path test_directory = "test_data";

    std::cout << "UATC filesystem test started." << std::endl;
    std::cout << "Scanning: " << test_directory << std::endl;

    try {
        // --------------------------------------------------
        // TEST 1: File enumeration
        // --------------------------------------------------

        auto files = uatc::FileEnumerator::enumerate(test_directory);

        std::cout << "\nFiles found: " << files.size() << "\n";

        for (const auto& file : files) {
            std::cout
                << "Path: " << file.relative_path.generic_string()
                << " | Size: " << file.file_size
                << " bytes"
                << std::endl;
        }

        std::set<std::string> expected_files = {
            "binary.bin",
            "data/students.txt",
            "empty.txt",
            "hello.txt",
            "marks.csv",
            "my test file.txt"
        };

        std::set<std::string> actual_files;

        for (const auto& file : files) {
            actual_files.insert(
                file.relative_path.generic_string()
            );
        }

        if (actual_files != expected_files) {
            std::cerr
                << "\nTEST FAILED: File list does not match."
                << std::endl;

            return 1;
        }

        std::cout
            << "\nTEST PASSED: All expected files were found."
            << std::endl;


        // --------------------------------------------------
        // TEST 2: Read normal text file
        // --------------------------------------------------

        auto hello_data =
            uatc::FileReader::read("test_data/hello.txt");

        std::string hello_content(
            hello_data.begin(),
            hello_data.end()
        );

        if (hello_content != "Hello UATC\r\n" &&
            hello_content != "Hello UATC\n") {

            std::cerr
                << "TEST FAILED: hello.txt content is incorrect."
                << std::endl;

            return 1;
        }

        std::cout
            << "TEST PASSED: FileReader read hello.txt."
            << std::endl;


        // --------------------------------------------------
        // TEST 3: Read empty file
        // --------------------------------------------------

        auto empty_data =
            uatc::FileReader::read("test_data/empty.txt");

        if (!empty_data.empty()) {

            std::cerr
                << "TEST FAILED: empty.txt should contain 0 bytes."
                << std::endl;

            return 1;
        }

        std::cout
            << "TEST PASSED: FileReader handled empty.txt."
            << std::endl;


        // --------------------------------------------------
        // TEST 4: Read binary file
        // --------------------------------------------------

        auto binary_data =
            uatc::FileReader::read("test_data/binary.bin");

        std::vector<std::uint8_t> expected_binary = {
            0, 1, 2, 3, 4, 5, 255
        };

        if (binary_data != expected_binary) {

            std::cerr
                << "TEST FAILED: binary.bin contents are incorrect."
                << std::endl;

            return 1;
        }

        std::cout
            << "TEST PASSED: FileReader handled binary.bin."
            << std::endl;


        // --------------------------------------------------
        // TEST 5: Filename containing spaces
        // --------------------------------------------------

        auto spaced_data =
            uatc::FileReader::read(
                "test_data/my test file.txt"
            );

        std::string spaced_content(
            spaced_data.begin(),
            spaced_data.end()
        );

        if (spaced_content !=
            "This file has spaces in its name.\r\n" &&
            spaced_content !=
            "This file has spaces in its name.\n") {

            std::cerr
                << "TEST FAILED: spaced filename content is incorrect."
                << std::endl;

            return 1;
        }

        std::cout
            << "TEST PASSED: FileReader handled filename with spaces."
            << std::endl;


        // --------------------------------------------------
        // ALL TESTS PASSED
        // --------------------------------------------------

        std::cout
            << "\nALL FILESYSTEM TESTS PASSED!"
            << std::endl;

        return 0;
    }

    catch (const std::exception& e) {

        std::cerr
            << "\nTEST FAILED: "
            << e.what()
            << std::endl;

        return 1;
    }
}