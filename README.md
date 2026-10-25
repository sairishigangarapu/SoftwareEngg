# SoftwareEngg — Universal Adaptive Transform Compression (UATC)

## Overview

**Universal Adaptive Transform Compression (UATC)** is a software engineering project focused on developing a lossless file compression and archival tool using C++17.

The project aims to organize files into blocks, apply reversible transformations, and store data in a versioned archive format. The archive and file-system components are designed to support reliable file handling, data integrity, and eventual integration with the compression pipeline.

## Objectives

- Develop a modular file compression and archival system.
- Support file and directory handling.
- Implement binary archive serialization and deserialization.
- Split file data into manageable blocks.
- Support data integrity checks and reliable archive reading.
- Use automated tests to validate individual modules.
- Apply software engineering practices such as modular design, version control, and build automation.

## Technologies Used

- **C++17** — Core implementation
- **CMake** — Build system
- **CTest** — Test execution
- **GoogleTest** — Unit testing
- **Git and GitHub** — Version control and collaboration

## Project Components

The repository includes development work related to:

- **File-system layer:** File and directory enumeration and reading.
- **Archive binary format:** Binary serialization and deserialization of archive data.
- **Block splitter:** Dividing file data into smaller blocks.
- **Archive writer and reader:** Writing archive data and reading it back.
- **Testing:** Automated tests for individual modules and archive operations.

The complete compression pipeline and other planned components should be considered implemented only when their functionality has been completed and verified.

## Repository Structure

The repository is organized into C++ source files, header files, build configuration, and tests. The exact directory structure can be inspected in the repository.

Typical components include:

- `include/` — Header files and interfaces
- `src/` — Implementation files
- `tests/` — Automated test cases
- `CMakeLists.txt` — CMake build configuration

## Prerequisites

Before building the project, install:

- A C++17-compatible compiler
- CMake
- Git
- GoogleTest, if required by the project's CMake configuration

## Build Instructions

Clone the repository:

```bash
git clone <YOUR_REPOSITORY_URL>
cd SoftwareEngg
```

Configure the project:

```bash
cmake -S . -B build
```

Build the project:

```bash
cmake --build build
```

## Running Tests

Run the configured test suite using CTest:

```bash
ctest --test-dir build --output-on-failure
```

Review the test output to identify any failures before considering the current implementation verified.

## Development Workflow

1. Create or switch to a development branch.
2. Implement a module or feature.
3. Build the project using CMake.
4. Run the relevant tests.
5. Commit the changes with a descriptive commit message.
6. Push the branch to GitHub and submit a pull request when required.

## Project Status

The repository contains development work on the file-system and archive components, including binary serialization and block splitting.

Implementation and testing of the overall system are ongoing. Features should be marked complete only after their implementation and tests have been verified.

## Contributors

This project is developed collaboratively as part of Software Engineering coursework at PES University.

## License

Refer to the repository's license file, if available, for the applicable usage and distribution terms.
