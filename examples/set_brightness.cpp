#include <iostream>
#include <string>

#include <cstdint>
#include <cstdlib>

#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>

    #pragma comment(lib, "Ws2_32.lib")
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
#endif

#include <ledriver.hpp>

namespace {

constexpr auto print_help = []() {
    std::cout << "Usage: (4|6) address port R G B" << std::endl;
    std::cout << "Values R, G and B must be in the range 0-65535" << std::endl;
};

} // namespace

int main(int argn, char* argv[]) {

    // Check argn.
    if (argn < 7) {
        print_help();
        return EXIT_FAILURE;
    }

    // Check the validity of the first argument (address family).
    if (std::string(argv[1]) != "4" && std::string(argv[1]) != "6") {
        print_help();
        return EXIT_FAILURE;
    }

    // Convert second and third arguments (address and port) to sockaddr.
    sockaddr_storage ss{};
    const int family = std::string(argv[1]) == "4" ? AF_INET : AF_INET6;
    if (family == AF_INET) {
        sockaddr_in* in = reinterpret_cast<sockaddr_in*>(&ss);
        in->sin_family = family;
        ::inet_pton(AF_INET, argv[2], &in->sin_addr);
        in->sin_port = ::htons(static_cast<std::uint16_t>(std::stoul(argv[3])));
    } else {
        sockaddr_in6* in6 = reinterpret_cast<sockaddr_in6*>(&ss);
        in6->sin6_family = family;
        ::inet_pton(AF_INET6, argv[2], &in6->sin6_addr);
        in6->sin6_port = ::htons(static_cast<std::uint16_t>(std::stoul(argv[3])));
    }

    // Get channel brightness values from arguments.
    const std::uint16_t R = static_cast<std::uint16_t>(std::stoul(argv[4]));
    const std::uint16_t G = static_cast<std::uint16_t>(std::stoul(argv[5]));
    const std::uint16_t B = static_cast<std::uint16_t>(std::stoul(argv[6]));

    // Create controller.
    LEDriver::Controller ctl(ss);

    // Update LED brightness.
    ctl.update(R, G, B);

    return EXIT_SUCCESS;
}