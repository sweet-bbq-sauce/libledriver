#include <chrono>
#include <limits>
#include <sys/types.h>
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
#include <tools.hpp>

#if defined(_WIN32)
namespace {
struct WSAInit {
    WSAInit() {
        WSADATA w{};
        if (const int result = ::WSAStartup(MAKEWORD(2, 2), &w); result != 0)
            throw std::system_error(result, std::system_category(), "WSAStartup");
    }

    ~WSAInit() {
        ::WSACleanup();
    }
};
} // namespace
#endif

bool LEDriver::ColorState::operator==(const ColorState& other) const noexcept {
    return r == other.r && g == other.g && b == other.b;
}

LEDriver::Controller::Controller(const sockaddr_storage& addr, std::chrono::milliseconds timeout) {

    // Require RootHeader to be 8 bytes.
    static_assert(sizeof(LEDriver::RootHeader) == 8, "RootHeader must be 8 bytes");

    // Support only IP4 and IP6.
    if (addr.ss_family != AF_INET && addr.ss_family != AF_INET6)
        throw std::system_error(EINVAL, std::generic_category());

    // If windows, initialize winsock.
#if defined(_WIN32)
    static WSAInit wsa;
#endif

    // Create UDP socket.
    fd_ = ::socket(addr.ss_family, SOCK_DGRAM, 0);
    if (fd_ == invalid_socket)
        throw std::system_error(GET_SOCKET_ERROR(), std::system_category(), "socket");

    // Use `connect()` on UDP, so we don't need to keep server address.
    if (::connect(fd_, reinterpret_cast<const sockaddr*>(&addr),
                  addr.ss_family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6)) != 0) {
        close();
        throw std::system_error(GET_SOCKET_ERROR(), std::system_category(), "connect");
    }

    const auto timeout_ms = timeout.count();

    // Set recv timeout.
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

LEDriver::Controller::Controller(Controller&& other) noexcept : fd_(std::exchange(other.fd_, invalid_socket)) {}

LEDriver::Controller& LEDriver::Controller::operator=(Controller&& other) noexcept {
    if (this == &other)
        return *this;

    close();
    fd_ = std::exchange(other.fd_, invalid_socket);

    return *this;
}

LEDriver::Controller::~Controller() noexcept {
    close();
}

void LEDriver::Controller::close() noexcept {
    if (fd_ == invalid_socket)
        return;

#if defined(_WIN32)
    ::closesocket(fd_);
#else
    ::close(fd_);
#endif

    fd_ = invalid_socket;
}

bool LEDriver::Controller::is_valid() const noexcept {
    return fd_ != invalid_socket;
}

LEDriver::Controller::operator bool() const noexcept {
    return is_valid();
}

void LEDriver::Controller::send_(const std::vector<std::span<const std::byte>>& data) {
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

std::size_t LEDriver::Controller::recv_(std::span<std::byte> data) {
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

    // Return the number of bytes received.
    return static_cast<std::size_t>(result);
}
