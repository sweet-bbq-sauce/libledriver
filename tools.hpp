#include <cstdint>

#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>

    #pragma comment(lib, "Ws2_32.lib")
#else
    #include <netinet/in.h>
#endif

#include <ledriver.hpp>

namespace {

//! Serialize `Action` to `u8`.
constexpr inline std::uint8_t SERIALIZE_ACTION(LEDriver::Action action) noexcept {
    return static_cast<std::uint8_t>(action);
};

//! Deserialize `u8` to `Action`.
constexpr inline LEDriver::Action DESERIALIZE_ACTION(std::uint8_t action) noexcept {
    return static_cast<LEDriver::Action>(action);
};

//! Serialize `u16` from host endian to network endian.
inline std::uint16_t SERIALIZE_U16(std::uint16_t u16_he) noexcept {
    return ::htons(u16_he);
};

//! Deserialize `u16` from network endian to host endian.
inline std::uint16_t DESERIALIZE_U16(std::uint16_t u16_ne) noexcept {
    return ::ntohs(u16_ne);
};

//! Serialize `u32` from host endian to network endian.
inline std::uint32_t SERIALIZE_U32(std::uint32_t u32_he) noexcept {
    return ::htonl(u32_he);
};

//! Deserialize `u32` from network endian to host endian.
inline std::uint32_t DESERIALIZE_U32(std::uint32_t u32_ne) noexcept {
    return ::ntohl(u32_ne);
};

//! Create `std::span<std::byte>` from any trivially copyable type object.
template <typename T> constexpr inline std::span<std::byte> TO_IOV(T& data) noexcept {
    return {reinterpret_cast<std::byte*>(&data), sizeof(data)};
}

//! Create `std::span<const std::byte>` from any trivially copyable type object.
template <typename T> constexpr inline std::span<const std::byte> TO_CIOV(const T& data) noexcept {
    return {reinterpret_cast<const std::byte*>(&data), sizeof(data)};
}

} // namespace