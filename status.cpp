#include <cstring>

#include <ledriver.hpp>
#include <tools.hpp>

LEDriver::Status LEDriver::Controller::status() {
    if (!is_valid())
        throw std::system_error(ENOTCONN, std::generic_category());

    RootHeader status_header{};
    status_header.magic = SERIALIZE_U32(RootHeader::magic_value);
    status_header.version = RootHeader::protocol_version;
    status_header.action = SERIALIZE_ACTION(Action::STATUS);
    status_header.flags = 0;

    send_({TO_CIOV(status_header)});

    std::byte buffer[sizeof(RootHeader) + 6 /* color state */ + 1 /* power state */];
    if (recv_({buffer}) != sizeof(buffer))
        throw std::system_error(EIO, std::generic_category());

    // TODO: check result header validity.

    std::uint16_t r_ne, g_ne, b_ne;
    std::uint8_t power;

    std::memcpy(&r_ne, buffer + 8, 2);
    std::memcpy(&g_ne, buffer + 10, 2);
    std::memcpy(&b_ne, buffer + 12, 2);
    std::memcpy(&power, buffer + 14, 1);

    ColorState color_state{DESERIALIZE_U16(r_ne), DESERIALIZE_U16(g_ne), DESERIALIZE_U16(b_ne)};

    return {color_state, static_cast<bool>(power)};
}