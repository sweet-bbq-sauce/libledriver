#include <chrono>
#include <cstdint>
#include <limits>
#include <system_error>
#include <utility>
#include <vector>

#include <cerrno>
#include <cstddef>
#include <cstring>

#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>

    #pragma comment(lib, "Ws2_32.lib")

    #define GET_SOCKET_ERROR() ::WSAGetLastError()
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <sys/uio.h>
    #include <unistd.h>

    #define GET_SOCKET_ERROR() errno
#endif

#include <ledriver.hpp>

#if defined(_WIN32)
namespace {
struct WSAInit {
    WSAInit() {
        WSADATA w{};
        const int result = ::WSAStartup(MAKEWORD(2, 2), &w);
        if (result != 0)
            throw std::system_error(result, std::system_category(), "WSAStartup");
    }

    ~WSAInit() {
        ::WSACleanup();
    }
};
} // namespace
#endif

namespace {

constexpr inline std::uint8_t SERIALIZE_ACTION(LEDriver::Action action) noexcept {
    return static_cast<std::uint8_t>(action);
};

constexpr inline LEDriver::Action DESERIALIZE_ACTION(std::uint8_t action) noexcept {
    return static_cast<LEDriver::Action>(action);
};

constexpr inline std::uint16_t SERIALIZE_U16(std::uint16_t u16_he) noexcept {
    return ::htons(u16_he);
};

constexpr inline std::uint16_t DESERIALIZE_U16(std::uint16_t u16_ne) noexcept {
    return ::ntohs(u16_ne);
};

constexpr inline std::uint32_t SERIALIZE_U32(std::uint32_t u32_he) noexcept {
    return ::htonl(u32_he);
};

constexpr inline std::uint32_t DESERIALIZE_U32(std::uint32_t u32_ne) noexcept {
    return ::ntohl(u32_ne);
};

template <typename T> constexpr inline std::span<std::byte> TO_IOV(T& data) noexcept {
    return {reinterpret_cast<std::byte*>(&data), sizeof(data)};
}

template <typename T> constexpr inline std::span<const std::byte> TO_CIOV(const T& data) noexcept {
    return {reinterpret_cast<const std::byte*>(&data), sizeof(data)};
}

} // namespace

LEDriver::Connector::Connector(const sockaddr_storage& addr, std::chrono::milliseconds timeout) {
    static_assert(sizeof(LEDriver::RootHeader) == 8, "RootHeader must be 8 bytes");

    if (addr.ss_family != AF_INET && addr.ss_family != AF_INET6)
        throw std::system_error(EINVAL, std::generic_category());

#if defined(_WIN32)
    static WSAInit wsa;
#endif

    fd_ = ::socket(addr.ss_family, SOCK_DGRAM, 0);
    if (fd_ == invalid_socket)
        throw std::system_error(GET_SOCKET_ERROR(), std::system_category(), "socket");

    if (::connect(fd_, reinterpret_cast<const sockaddr*>(&addr),
                  addr.ss_family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6)) != 0) {
        close();
        throw std::system_error(GET_SOCKET_ERROR(), std::system_category(), "connect");
    }

    const auto timeout_ms = timeout.count();

#if defined(_WIN32)
    DWORD ms;
    if (timeout_ms <= 0)
        ms = 0;
    else if (timeout_ms > static_cast<decltype(timeout_ms)>((std::numeric_limits<DWORD>::max)()))
        ms = (std::numeric_limits<DWORD>::max)();
    else
        ms = static_cast<DWORD>(timeout_ms);

    if (::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&ms), sizeof(ms)) != 0) {
        close();
        throw std::system_error(GET_SOCKET_ERROR(), std::system_category(), "setsockopt");
    }
#else
    timeval tv{};
    if (timeout_ms > 0) {
        tv.tv_sec = static_cast<decltype(tv.tv_sec)>(timeout_ms / 1000);
        tv.tv_usec = static_cast<decltype(tv.tv_usec)>((timeout_ms % 1000) * 1000);
    }

    if (::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
        close();
        throw std::system_error(GET_SOCKET_ERROR(), std::system_category(), "setsockopt");
    }
#endif
}

LEDriver::Connector::Connector(Connector&& other) noexcept : fd_(std::exchange(other.fd_, invalid_socket)) {}

LEDriver::Connector& LEDriver::Connector::operator=(Connector&& other) noexcept {
    if (this == &other)
        return *this;

    close();
    fd_ = std::exchange(other.fd_, invalid_socket);

    return *this;
}

LEDriver::Connector::~Connector() noexcept {
    close();
}

void LEDriver::Connector::close() noexcept {
    if (fd_ == invalid_socket)
        return;

#if defined(_WIN32)
    ::closesocket(fd_);
#else
    ::close(fd_);
#endif

    fd_ = invalid_socket;
}

bool LEDriver::Connector::ping() {
    if (!is_valid())
        throw std::system_error(ENOTCONN, std::generic_category());

    RootHeader ping_header{}, pong_header;
    ping_header.magic = SERIALIZE_U32(RootHeader::magic_value);
    ping_header.version = RootHeader::protocol_version;
    ping_header.action = SERIALIZE_ACTION(Action::PING);
    ping_header.flags = 0;

    send_({TO_CIOV(ping_header)});

    try {

        if (recv_({TO_IOV(pong_header)}) != sizeof(RootHeader))
            throw std::system_error(EIO, std::generic_category());

    } catch (const std::system_error& se) {

        const int code = se.code().value();
#if defined(_WIN32)
        if (code == WSAETIMEDOUT || code == WSAEWOULDBLOCK)
            return false;
#else
        if (code == ETIMEDOUT || code == EWOULDBLOCK || code == EAGAIN)
            return false;
#endif

        throw;
    }

    return ping_header.magic == pong_header.magic && ping_header.version == pong_header.version &&
           ping_header.action == pong_header.action && ping_header.flags == pong_header.flags;
}

void LEDriver::Connector::update(std::uint16_t r, std::uint16_t g, std::uint16_t b) {
    if (!is_valid())
        throw std::system_error(ENOTCONN, std::generic_category());

    RootHeader update_header{};
    update_header.magic = SERIALIZE_U32(RootHeader::magic_value);
    update_header.version = RootHeader::protocol_version;
    update_header.action = SERIALIZE_ACTION(Action::UPDATE);
    update_header.flags = 0;

    const std::uint16_t values[3]{SERIALIZE_U16(r), SERIALIZE_U16(g), SERIALIZE_U16(b)};

    send_({TO_CIOV(update_header), TO_CIOV(values)});
}

bool LEDriver::Connector::is_valid() const noexcept {
    return fd_ != invalid_socket;
}

LEDriver::Connector::operator bool() const noexcept {
    return is_valid();
}

void LEDriver::Connector::send_(const std::vector<std::span<const std::byte>>& data) {
    if (!is_valid())
        throw std::system_error(ENOTCONN, std::generic_category());

    if (data.empty())
        throw std::system_error(EINVAL, std::generic_category());

    std::size_t to_send{};

#if defined(_WIN32)
    if (data.size() > (std::numeric_limits<DWORD>::max)())
        throw std::system_error(EINVAL, std::generic_category());

    std::vector<WSABUF> segments;
    for (const auto& segment : data) {
        if (segment.size() > (std::numeric_limits<decltype(WSABUF::len)>::max)())
            throw std::system_error(EINVAL, std::generic_category());

        WSABUF buffer{};
        buffer.buf = reinterpret_cast<CHAR*>(const_cast<std::byte*>(segment.data()));
        buffer.len = static_cast<decltype(WSABUF::len)>(segment.size());
        segments.push_back(buffer);
        to_send += segment.size();
    }

    if (to_send == 0)
        throw std::system_error(EINVAL, std::generic_category());

    DWORD sent;
    if (WSASend(fd_, segments.data(), static_cast<DWORD>(segments.size()), &sent, 0, nullptr, nullptr) == SOCKET_ERROR)
        throw std::system_error(GET_SOCKET_ERROR(), std::system_category(), "WSASend");

    if (static_cast<std::size_t>(sent) != to_send)
        throw std::system_error(EIO, std::generic_category());
#else
    std::vector<iovec> segments;
    for (const auto& segment : data) {
        if (segment.size() > (std::numeric_limits<decltype(iovec::iov_len)>::max)())
            throw std::system_error(EINVAL, std::generic_category());

        iovec buffer{};
        buffer.iov_base = static_cast<void*>(const_cast<std::byte*>(segment.data()));
        buffer.iov_len = static_cast<decltype(iovec::iov_len)>(segment.size());
        segments.push_back(buffer);
        to_send += segment.size();
    }

    if (to_send == 0)
        throw std::system_error(EINVAL, std::generic_category());

    const msghdr hdr{.msg_iov = segments.data(), .msg_iovlen = segments.size()};
    const ssize_t result = ::sendmsg(fd_, &hdr, 0);
    if (result < 0)
        throw std::system_error(GET_SOCKET_ERROR(), std::system_category(), "sendmsg");
    else if (static_cast<std::size_t>(result) != to_send)
        throw std::system_error(EIO, std::generic_category());
#endif
}

std::size_t LEDriver::Connector::recv_(std::span<std::byte> data) {
    if (!is_valid())
        throw std::system_error(ENOTCONN, std::generic_category());

    if (data.empty())
        throw std::system_error(EINVAL, std::generic_category());

#if defined(_WIN32)
    if (data.size() > (std::numeric_limits<int>::max)())
        throw std::system_error(EINVAL, std::generic_category());

    const int result = ::recv(fd_, reinterpret_cast<CHAR*>(data.data()), static_cast<int>(data.size()), 0);
    if (result == SOCKET_ERROR)
        throw std::system_error(GET_SOCKET_ERROR(), std::system_category(), "recv");
#else
    const ssize_t result = ::recv(fd_, reinterpret_cast<void*>(data.data()), static_cast<size_t>(data.size()), 0);
    if (result < 0)
        throw std::system_error(GET_SOCKET_ERROR(), std::system_category(), "recv");
#endif

    return static_cast<std::size_t>(result);
}
