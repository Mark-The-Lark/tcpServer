#include "../Common/net_utils.h"
#include "Client.h"
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <thread>
#include <atomic>



std::atomic<bool> running{ true };

void receive_thread(net_utils::socket_t server_socket) {
    while (running) {
        std::string message = net_utils::read_message(server_socket);
        if (message.empty()) {
            std::cout << "\n Connection lost!" << std::endl;
            running = false;
            break;
        }

        // Выводим сообщение с новой строки
        std::cout << "\n" << message << std::endl;
        std::cout << "> " << std::flush;
    }
}


net_utils::socket_t connectToServer(std::string IP) {
    #ifdef _WIN32
    // Устанавливаем UTF-8 для консоли Windows
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Настраиваем шрифт, поддерживающий Unicode
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_FONT_INFOEX fontInfo;
    fontInfo.cbSize = sizeof(fontInfo);
    GetCurrentConsoleFontEx(hConsole, FALSE, &fontInfo);
    wcscpy_s(fontInfo.FaceName, L"Consolas");
    SetCurrentConsoleFontEx(hConsole, FALSE, &fontInfo);
    #endif

    if (!net_utils::net_init()) {
        std::cerr << "Network init failed!" << std::endl;
        return 1;
    }

    // Создаём сокет
    net_utils::socket_t clientSocket = net_utils::create_tcp_socket();
    if (clientSocket == SOCKET_ERROR_VAL) {
        std::cerr << "Socket creation failed: " << net_utils::get_last_error() << std::endl;
        net_utils::net_cleanup();
        return 1;
    }

    // Настраиваем адрес сервера
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);
    inet_pton(AF_INET, IP.c_str(), &server_addr.sin_addr);


    // Подключаемся к серверу
    if (connect(clientSocket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR_VAL) {
        std::cerr << "Connect failed: " << net_utils::get_last_error() << std::endl;
        net_utils::socket_close(clientSocket);
        net_utils::net_cleanup();
        return 1;
    }
    return clientSocket;
}


int ClientListener::runClient(const std::string& IP) {

    std::cout << "Connected to server!" << std::endl;
    net_utils::socket_t clientSocket = connectToServer(IP);
    std::thread receiver(receive_thread, clientSocket);

    std::string input;
    while (running) {
        std::cout << "> ";
        std::getline(std::cin, input);

        if (!running) break;
        if (input.empty()) continue;

        // Отправляем сообщение
        if (!net_utils::send_message(clientSocket, input)) {
            std::cout << "Ошибка отправки сообщения!" << std::endl;
            break;
        }

        // Проверяем на выход
        if (input == "/exit") {
            running = false;
            break;
        }
    }
    running = false;
    receiver.join();

    // Закрываем соединение
    net_utils::shutdown(clientSocket);

    net_utils::socket_close(clientSocket);

    net_utils::net_cleanup();


    std::cout << "Press Enter to exit...";
    std::cin.get();

    std::cout << "Клиент завершил работу." << std::endl;
    return 0;
}

std::string validateIP(std::string ip) {
    if (ip == "localhost" || ip.empty()) return "127.0.0.1";
    std::stringstream ss(ip);
    std::string segment;
    std::vector<std::string> parts;
    parts.reserve(4);
    while (std::getline(ss, segment, '.')) {
        parts.push_back(segment); // Добавляем часть в вектор
    }
    if (parts.size() != 4) return "1";
    for (int i = 0; i < 4; ++i) {
        std::string s = parts[i];
        if (s.empty() || !std::all_of(s.begin(), s.end(), ::isdigit)) return "2";
    }
    return ip;
}

