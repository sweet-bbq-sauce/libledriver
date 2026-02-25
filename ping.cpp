#include <ledriver.hpp>
#include <tools.hpp>

bool LEDriver::Controller::ping() {
    if (!is_valid())
        throw std::system_error(ENOTCONN, std::generic_category());

    RootHeader ping_header{}, pong_header;
    ping_header.magic = SERIALIZE_U32(RootHeader::magic_value);
    ping_header.version = RootHeader::protocol_version;
    ping_header.action = SERIALIZE_ACTION(Action::PING);
    ping_header.flags = 0;

    // Send PING frame to driver.
    send_({TO_CIOV(ping_header)});

    try {

        // Wait for a PONG frame from the server.
        if (recv_({TO_IOV(pong_header)}) != sizeof(RootHeader))
            throw std::system_error(EIO, std::generic_category());

    } catch (const std::system_error& se) {

        // If the recv error is due to timeout, return false.
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

    // PING and PONG frames must be the same.
    return ping_header.magic == pong_header.magic && ping_header.version == pong_header.version &&
           ping_header.action == pong_header.action && ping_header.flags == pong_header.flags;
}