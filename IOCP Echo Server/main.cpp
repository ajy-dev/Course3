#include "IOCPEchoServer.hpp"
#include "Logger.hpp"
#include <iostream>

int main()
{
        const std::uint16_t port = 6000;

        IOCPEchoServer server(port);

        if (!server.start())
        {
                std::cerr << "Server start failed\n";
                return 1;
        }

        std::cout << "IOCP Echo Server started. Press ENTER to shutdown.\n";

        std::cin.get();

        server.shutdown();

        std::cout << "Server shutdown complete.\n";

        return 0;
}