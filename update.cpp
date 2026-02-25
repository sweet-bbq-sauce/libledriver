#include <ledriver.hpp>
#include <tools.hpp>

void LEDriver::Controller::update(const ColorState& state) {
    if (!is_valid())
        throw std::system_error(ENOTCONN, std::generic_category());

    // If the color state persists, do not send.
    if (state == color_state_cache_)
        return;

    RootHeader update_header{};
    update_header.magic = SERIALIZE_U32(RootHeader::magic_value);
    update_header.version = RootHeader::protocol_version;
    update_header.action = SERIALIZE_ACTION(Action::UPDATE);
    update_header.flags = 0;

    // UPDATE action requires 6-byte payload (3 * u16), containing the channel brightness values ​​in net endian.
    const std::uint16_t values[3]{SERIALIZE_U16(state.r), SERIALIZE_U16(state.g), SERIALIZE_U16(state.b)};

    send_({TO_CIOV(update_header), TO_CIOV(values)});

    color_state_cache_ = state;
}