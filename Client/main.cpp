#include "Client.h"
#include "ClientUDP.h"

#include <iostream>
#include <string>
#include <Windows.h>

int main() {
    std::string ip = "localhost";
    std::cout << "Enter an ip of server: ";
    std::getline(std::cin, ip);
    Sleep(1000);
    ip = validateIP(ip);
    #ifdef TCP
    try {
        ClientListener client;
        client.runClient(ip.c_str());
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    #else
    try {
        UdpRadioClient client(ip);
        client.start();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    #endif

    return 0;
}