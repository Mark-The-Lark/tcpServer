#include "../Common/net_utils.h"
#include "Server.h"
#include <string>

int runServer() {

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

    net_utils::net_init();  // Инициализация сетевой подсистемы (для Windows обязательно)

    // Создание сокета
    net_utils::socket_t serverSocket = net_utils::create_tcp_socket();
    if (serverSocket == net_utils::INVALID_SOCKET_VAL) {
        std::cerr << "Error socket initialization: " << net_utils::get_last_error() << std::endl;
        net_utils::net_cleanup();
        return 1;
    }

    // Настройка адреса сервера
    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; // Принимать подключения с любых адресов
    serverAddr.sin_port = htons(12345);      // Порт 12345

    // Привязка сокета к адресу
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) != 0) {
        std::cerr << "Ошибка привязки: " << net_utils::get_last_error() << std::endl;
        net_utils::socket_close(serverSocket);
        net_utils::net_cleanup();
        return 1;
    }

    // Начинаем слушать подключения
    if (listen(serverSocket, 5) != 0) {
        std::cerr << "Error listen: " << net_utils::get_last_error() << std::endl;
        net_utils::socket_close(serverSocket);
        net_utils::net_cleanup();
        return 1;
    }

    std::cout << "Server started. Waiting for connection to port 12345..." << std::endl;

    // Принимаем подключение
    sockaddr_in clientAddr;
    #ifdef _WIN32
    int clientLen = sizeof(clientAddr);
    #else
    socklen_t clientLen = sizeof(clientAddr);
    #endif

    net_utils::socket_t clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);

    if (clientSocket == net_utils::INVALID_SOCKET_VAL) {
        std::cerr << "Error accept: " << net_utils::get_last_error() << std::endl;
        net_utils::socket_close(serverSocket);
        net_utils::net_cleanup();
        return 1;
    }

    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
    std::cout << "Client connected: " << clientIP << ":" << ntohs(clientAddr.sin_port) << std::endl;

    // Читаем данные от клиента
    std::string buffer = net_utils::read_message(clientSocket);
    int bytesRead = buffer.size();


    if (bytesRead > 0) {
        std::cout << "Listened from client (" << bytesRead << " bytes): " << buffer << std::endl;

        // Отправляем ответ
        std::string response = "Hi from server! Listened: " + std::to_string(bytesRead) + " bytes";


        net_utils::send_message(clientSocket, response.c_str());
    }
    else if (bytesRead == 0) {
        std::cout << "Client disconnected" << std::endl;
    }
    else {
        std::cerr << "Read error: " << net_utils::get_last_error() << std::endl;
    }

    // Корректно закрываем соединение
    net_utils::shutdown(clientSocket);

    net_utils::socket_close(clientSocket);
    net_utils::socket_close(serverSocket);

    net_utils::net_cleanup();  // Для Windows важно!

    std::cout << "Press Enter to exit...";
    std::cin.get();

    return 0;
}