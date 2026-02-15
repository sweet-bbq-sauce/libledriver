#pragma once

#include <chrono>
#include <span>
#include <vector>

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>

    #pragma comment(lib, "Ws2_32.lib")
#else
    #include <sys/socket.h>
#endif

namespace LEDriver {

enum class Action : std::uint8_t { NONE = 0x00, PING = 0x01, UPDATE = 0x02 };

struct RootHeader {
    std::uint32_t magic;
    std::uint8_t version;
    std::uint8_t action;
    std::uint16_t flags;

    static constexpr std::uint32_t magic_value = 0x4C454452;    // "LEDR"
    static constexpr std::uint8_t protocol_version = 0x00;      // unstable/dev version
};

class Connector {
  public:
    Connector() noexcept = default;
    explicit Connector(const sockaddr_storage& addr,
                       std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

    Connector(Connector&&) noexcept;
    Connector& operator=(Connector&&) noexcept;
    Connector(const Connector&) = delete;
    Connector& operator=(const Connector&) = delete;
    ~Connector() noexcept;

    void close() noexcept;

    bool ping();
    void update(std::uint16_t r, std::uint16_t g, std::uint16_t b);

    bool is_valid() const noexcept;
    explicit operator bool() const noexcept;

  private:
#if defined(_WIN32)
    using socket_t = SOCKET;
    static constexpr socket_t invalid_socket = INVALID_SOCKET;
#else
    using socket_t = int;
    static constexpr socket_t invalid_socket = -1;
#endif

    socket_t fd_{invalid_socket};

    void send_(const std::vector<std::span<const std::byte>>& data);
    std::size_t recv_(std::span<std::byte> data);
};

} // namespace LEDriver