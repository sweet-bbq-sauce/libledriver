/*!
    \file
    \brief Main header containing main class `Controller` and `RootHeader` struct.
    \copyright Copyright (c) 2026 Wiktor Sołtys
    \cond
        See LICENSE
    \endcond
*/
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

/*!
    The action opcode. It's located in 'RootHeader::action'.
    In a client request, it tells the server what action we want to obtain.
    In the server's response, it indicates the response context.
*/
enum class Action : std::uint8_t {
    NONE = 0x00,  //!< No action. The server ignores the command and does not send any response.
    PING = 0x01,  //!< Ping-pong. The server sends back as a response the same header as the one sent by the client.
                  //!< See `Controller::ping()`.
    UPDATE = 0x02 //!< Update LED state. See `Controller::update()`.
};

//! `RootHeader` is the main header of each frame used in driver-client communication.
struct RootHeader {
    std::uint32_t magic;  //!< Magic value. See `RootHeader::magic_value`.
    std::uint8_t version; //!< Protocol version. See `RootHeader::protocol_version`.
    std::uint8_t action;  //!< Action opcode. See `LEDriver::Action`.
    std::uint16_t flags;  //!< Header flags. Currently not in use yet.

    static constexpr std::uint32_t magic_value = 0x4C454452; //!< `"LEDR"`
    static constexpr std::uint8_t protocol_version = 0x00;   //!< unstable/dev version
};

//! A move-only class that allows connectionless (UDP) communication with the driver.
class Controller {
  public:
    //! Create an empty, invalid Controller.
    Controller() noexcept = default;

    /*!
        Create UDP socket for communication with driver.

        \param addr - IPv4/IPv6 address of the driver.
        \param timeout - time in ms after which no response from the driver indicates a network error/no contact.
                         Affects methods that require a response from the controller, e.g. `Controller::ping()`.
    */
    explicit Controller(const sockaddr_storage& addr,
                        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

    Controller(Controller&&) noexcept;
    Controller& operator=(Controller&&) noexcept;
    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;
    ~Controller() noexcept;

    //! Close the socket and set the Controller status to closed.
    void close() noexcept;

    /*!
        \brief Send a PING frame to the driver and wait for a PONG frame.

        \return `true` when the driver returns correct PONG frame within the `timeout` specified in the constructor.
        \return `false` when the driver does not return PONG within the `timeout` specified in the constructor, or the
                PONG frame is incorrect.

        \throw std::system_error
               - `ENOTCONN` when Controller is not valid (closed)
               - `EIO` when server response has invalid size
               - system network layer errors
    */
    bool ping();

    /*!
        \brief Update the LED status in the driver. The driver does not return any response.
               Each channel takes on a 16-bit value, specifying the channel's brightness.

        \param r - red channel
        \param g - green channel
        \param b - blue channel

        \throw std::system_error
               - `ENOTCONN` when Controller is not valid (closed)
               - system network layer errors
    */
    void update(std::uint16_t r, std::uint16_t g, std::uint16_t b);

    //! \return `true` when Controller is valid (not closed).
    bool is_valid() const noexcept;

    /*!
        \brief Alias for `Controller::is_valid()`.
        \return `true` when Controller is valid (not closed).
    */
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