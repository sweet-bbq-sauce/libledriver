#include <ledriver.hpp>
#include <tools.hpp>

void LEDriver::Controller::power(bool state) {
    if (!is_valid())
        throw std::system_error(ENOTCONN, std::generic_category());

    RootHeader power_header{};
    power_header.magic = SERIALIZE_U32(RootHeader::magic_value);
    power_header.version = RootHeader::protocol_version;
    power_header.action = SERIALIZE_ACTION(Action::POWER);
    power_header.flags = 0;

    // POWER action requires u8 value, containing 0x00 (OFF) or 0x01 (ON).
    send_({TO_CIOV(power_header), TO_CIOV(static_cast<std::uint8_t>(state))});
}