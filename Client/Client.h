#include <string>
#pragma once

class ClientListener {
public:
	int runClient(const std::string& IP);
};
std::string validateIP(std::string ip);