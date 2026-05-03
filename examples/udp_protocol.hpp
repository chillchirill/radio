#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace udp_example {

constexpr std::string_view kAcceptedResponse = "OK";
constexpr std::string_view kRejectedResponse = "ERR";
constexpr std::size_t kDataPacketSize =
    sizeof(std::uint16_t) * 4 + sizeof(std::uint32_t);

using PacketBuffer = std::array<std::uint8_t, kDataPacketSize>;

struct Payload {
    std::uint16_t value1 = 0;
    std::uint16_t value2 = 0;
    std::uint16_t value3 = 0;
    std::uint16_t value4 = 0;
    std::uint32_t elapsed_ms = 0;
};

inline void StoreUint16(PacketBuffer& buffer, std::size_t offset, std::uint16_t value) {
    buffer[offset] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    buffer[offset + 1] = static_cast<std::uint8_t>(value & 0xFF);
}

inline void StoreUint32(PacketBuffer& buffer, std::size_t offset, std::uint32_t value) {
    buffer[offset] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
    buffer[offset + 1] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    buffer[offset + 2] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    buffer[offset + 3] = static_cast<std::uint8_t>(value & 0xFF);
}

inline std::uint16_t LoadUint16(const PacketBuffer& buffer, std::size_t offset) {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(buffer[offset]) << 8) |
        static_cast<std::uint16_t>(buffer[offset + 1]));
}

inline std::uint32_t LoadUint32(const PacketBuffer& buffer, std::size_t offset) {
    return (static_cast<std::uint32_t>(buffer[offset]) << 24) |
           (static_cast<std::uint32_t>(buffer[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(buffer[offset + 2]) << 8) |
           static_cast<std::uint32_t>(buffer[offset + 3]);
}

inline PacketBuffer EncodePayload(const Payload& payload) {
    PacketBuffer buffer{};
    StoreUint16(buffer, 0, payload.value1);
    StoreUint16(buffer, 2, payload.value2);
    StoreUint16(buffer, 4, payload.value3);
    StoreUint16(buffer, 6, payload.value4);
    StoreUint32(buffer, 8, payload.elapsed_ms);
    return buffer;
}

inline Payload DecodePayload(const PacketBuffer& buffer) {
    Payload payload;
    payload.value1 = LoadUint16(buffer, 0);
    payload.value2 = LoadUint16(buffer, 2);
    payload.value3 = LoadUint16(buffer, 4);
    payload.value4 = LoadUint16(buffer, 6);
    payload.elapsed_ms = LoadUint32(buffer, 8);
    return payload;
}

inline std::uint32_t ClampToUint32(unsigned long long value) {
    const auto max_value = static_cast<unsigned long long>(std::numeric_limits<std::uint32_t>::max());
    if (value > max_value) {
        return std::numeric_limits<std::uint32_t>::max();
    }

    return static_cast<std::uint32_t>(value);
}

}  // namespace udp_example
