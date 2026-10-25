
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "filesystem/safe_path.hpp"

int main() {
    namespace fs = std::filesystem;

    const fs::path root = "path_test_output";

    try {
        // Normal nested paths must be accepted.
        const fs::path normal =
            uatc::safe_destination(root, "subfolder/file.txt");

        if (normal.filename() != "file.txt" ||
            normal.parent_path().filename() != "subfolder") {
            throw std::runtime_error(
                "A normal relative path was changed unexpectedly."
            );
        }

        // Traversal and rooted paths must be rejected.
        const std::vector<fs::path> unsafe_paths = {
            "../outside.txt",
            "subfolder/../../outside.txt",
            "..",
            ".",
            fs::path("/outside.txt"),
            fs::path("C:/outside.txt")
        };

        for (const auto& unsafe_path : unsafe_paths) {
            bool rejected = false;

            try {
                (void)uatc::safe_destination(root, unsafe_path);
            } catch (const std::runtime_error&) {
                rejected = true;
            }

            if (!rejected) {
                throw std::runtime_error(
                    "Unsafe path was accepted: " + unsafe_path.string()
                );
            }
        }

        // A simple filename must still work.
        const fs::path simple =
            uatc::safe_destination(root, "hello.txt");

        if (simple.filename() != "hello.txt") {
            throw std::runtime_error(
                "A valid filename was not preserved."
            );
        }

        std::cout
            << "PASSED: valid paths accepted and traversal paths rejected.\n";

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
