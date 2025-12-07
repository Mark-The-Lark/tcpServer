#include "Client.h"

#include <iostream>
#include <string>
#include <Windows.h>

int main() {
    std::string ip = "localhost";
    std::cout << "Enter an ip of server: ";
    std::getline(std::cin, ip);
    Sleep(1000);
    ip = validateIP(ip);
    runClient(ip.c_str());

    return 0;
}