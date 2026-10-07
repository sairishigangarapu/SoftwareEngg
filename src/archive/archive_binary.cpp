#include "archive/archive_binary.hpp"

#include <stdexcept>

namespace uatc {

// ------------------------------------------------------------
// BinaryWriter
// ------------------------------------------------------------

BinaryWriter::BinaryWriter(
    const std::filesystem::path& path
)
    : file_(path, std::ios::binary) {

    if (!file_) {
        throw std::runtime_error(
            "Could not open file for writing: " + path.string()
        );
    }
}

BinaryWriter::~BinaryWriter() {
    file_.close();
}

void BinaryWriter::write_u8(std::uint8_t value) {
    file_.put(static_cast<char>(value));

    if (!file_) {
        throw std::runtime_error(
            "Failed to write byte."
        );
    }
}

void BinaryWriter::write_u16(std::uint16_t value) {
    write_u8(
        static_cast<std::uint8_t>(value & 0xFF)
    );

    write_u8(
        static_cast<std::uint8_t>((value >> 8) & 0xFF)
    );
}

void BinaryWriter::write_u32(std::uint32_t value) {
    write_u8(
        static_cast<std::uint8_t>(value & 0xFF)
    );

    write_u8(
        static_cast<std::uint8_t>((value >> 8) & 0xFF)
    );

    write_u8(
        static_cast<std::uint8_t>((value >> 16) & 0xFF)
    );

    write_u8(
        static_cast<std::uint8_t>((value >> 24) & 0xFF)
    );
}

void BinaryWriter::write_u64(std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        write_u8(
            static_cast<std::uint8_t>(
                (value >> (i * 8)) & 0xFF
            )
        );
    }
}

void BinaryWriter::write_bytes(
    const std::vector<std::uint8_t>& data
) {
    if (data.empty()) {
        return;
    }

    file_.write(
        reinterpret_cast<const char*>(data.data()),
        static_cast<std::streamsize>(data.size())
    );

    if (!file_) {
        throw std::runtime_error(
            "Failed to write byte data."
        );
    }
}

void BinaryWriter::write_string(
    const std::string& value
) {
    if (value.empty()) {
        return;
    }

    file_.write(
        value.data(),
        static_cast<std::streamsize>(value.size())
    );

    if (!file_) {
        throw std::runtime_error(
            "Failed to write string."
        );
    }
}

std::uint64_t BinaryWriter::position() {
    return static_cast<std::uint64_t>(
        file_.tellp()
    );
}

// ------------------------------------------------------------
// BinaryReader
// ------------------------------------------------------------

BinaryReader::BinaryReader(
    const std::filesystem::path& path
)
    : file_(path, std::ios::binary) {

    if (!file_) {
        throw std::runtime_error(
            "Could not open file for reading: " + path.string()
        );
    }
}

BinaryReader::~BinaryReader() {
    file_.close();
}

std::uint8_t BinaryReader::read_u8() {
    char byte = 0;

    if (!file_.get(byte)) {
        throw std::runtime_error(
            "Unexpected end of archive."
        );
    }

    return static_cast<std::uint8_t>(
        static_cast<unsigned char>(byte)
    );
}

std::uint16_t BinaryReader::read_u16() {
    std::uint16_t value = 0;

    value |= static_cast<std::uint16_t>(read_u8());

    value |= static_cast<std::uint16_t>(
        read_u8()
    ) << 8;

    return value;
}

std::uint32_t BinaryReader::read_u32() {
    std::uint32_t value = 0;

    value |= static_cast<std::uint32_t>(
        read_u8()
    );

    value |= static_cast<std::uint32_t>(
        read_u8()
    ) << 8;

    value |= static_cast<std::uint32_t>(
        read_u8()
    ) << 16;

    value |= static_cast<std::uint32_t>(
        read_u8()
    ) << 24;

    return value;
}

std::uint64_t BinaryReader::read_u64() {
    std::uint64_t value = 0;

    for (int i = 0; i < 8; ++i) {
        value |=
            static_cast<std::uint64_t>(read_u8())
            << (i * 8);
    }

    return value;
}

std::vector<std::uint8_t> BinaryReader::read_bytes(
    std::size_t count
) {
    std::vector<std::uint8_t> data(count);

    if (count == 0) {
        return data;
    }

    file_.read(
        reinterpret_cast<char*>(data.data()),
        static_cast<std::streamsize>(count)
    );

    if (!file_) {
        throw std::runtime_error(
            "Unexpected end of archive while reading bytes."
        );
    }

    return data;
}

std::string BinaryReader::read_string(
    std::size_t count
) {
    const auto data = read_bytes(count);

    return std::string(
        data.begin(),
        data.end()
    );
}

std::uint64_t BinaryReader::position() {
    return static_cast<std::uint64_t>(
        file_.tellg()
    );
}

}