#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace uatc {

class BinaryWriter {
public:
    explicit BinaryWriter(const std::filesystem::path& path);

    ~BinaryWriter();

    void write_u8(std::uint8_t value);
    void write_u16(std::uint16_t value);
    void write_u32(std::uint32_t value);
    void write_u64(std::uint64_t value);

    void write_bytes(const std::vector<std::uint8_t>& data);
    void write_string(const std::string& value);

    std::uint64_t position();

private:
    std::ofstream file_;
};

class BinaryReader {
public:
    explicit BinaryReader(const std::filesystem::path& path);

    ~BinaryReader();

    std::uint8_t read_u8();
    std::uint16_t read_u16();
    std::uint32_t read_u32();
    std::uint64_t read_u64();

    std::vector<std::uint8_t> read_bytes(std::size_t count);
    std::string read_string(std::size_t count);

    std::uint64_t position();

private:
    std::ifstream file_;
};

}