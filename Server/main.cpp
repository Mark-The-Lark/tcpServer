#include "Server.h"
#include "ServerUDP.h"

#include <iostream>
#include <string>
#include <Windows.h>

int main() {
    #ifdef TCP
    try {
    runServer();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    #else
    try {
        UdpRadioServer server;
        server.start();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    #endif

    return 0;
}