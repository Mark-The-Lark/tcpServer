#include "../Common/net_utils.h"
#include <iostream>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>

class ClientManager {
private:
    struct Client {
        net_utils::socket_t socket;            // Сокет клиента
        struct sockaddr_in address; // Адрес клиента
        std::string name;           // Имя клиента
        int id;                     // Уникальный ID
        bool connected;             // Статус подключения
    };

    // Глобальные переменные (для простоты)
    std::map<int, Client> clients_;      // Все клиенты
    std::mutex clients_mutex_;           // Защита доступа к клиентам
    std::atomic<int> next_client_id_{ 1 }; // Счётчик ID
public:
    int add_client(net_utils::socket_t socket, struct sockaddr_in address) {
        std::lock_guard<std::mutex> lock(clients_mutex_);

        int new_id = next_client_id_++;
        Client new_client;
        new_client.socket = socket;
        new_client.address = address;
        new_client.name = "User" + std::to_string(new_id);
        new_client.id = new_id;
        new_client.connected = true;

        clients_[new_id] = new_client;
        return new_id;
    }

    // Удалить клиента
    void remove_client(int client_id) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(client_id);
        if (it != clients_.end()) {
            clients_.erase(it);
        }
    }

    // Отключить клиента (но не удалять сразу)
    void disconnect_client(int client_id) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(client_id);
        if (it != clients_.end()) {
            it->second.connected = false;
        }
    }

    // Получить имя клиента
    std::string get_client_name(int client_id) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(client_id);
        if (it != clients_.end()) {
            return it->second.name;
        }
        return "Unknown";
    }

    // Установить имя клиента
    void set_client_name(int client_id, const std::string& name) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(client_id);
        if (it != clients_.end()) {
            it->second.name = name;
        }
    }

    // Получить сокет клиента
    net_utils::socket_t get_client_socket(int client_id) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(client_id);
        if (it != clients_.end()) {
            return it->second.socket;
        }
        return net_utils::INVALID_SOCKET_VAL;
    }

    // Проверить подключен ли клиент
    bool is_client_connected(int client_id) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(client_id);
        return it != clients_.end() && it->second.connected;
    }

    // Получить список всех подключенных клиентов
    std::vector<int> get_connected_clients() {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        std::vector<int> result;

        for (const auto& pair : clients_) {
            if (pair.second.connected) {
                result.push_back(pair.first);
            }
        }

        return result;
    }

    // Получить количество клиентов
    size_t get_client_count() {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        return clients_.size();
    }
    void broadcast_message(const std::string& message, int exclude_id = -1) {
        std::lock_guard<std::mutex> lock(clients_mutex_);

        for (auto& pair : clients_) {
            Client& client = pair.second;

            // Не отправляем исключённому клиенту
            if (client.id == exclude_id) continue;

            // Не отправляем отключённым клиентам
            if (!client.connected) continue;

            // Пытаемся отправить
            if (!net_utils::send_message(client.socket, message)) {
                // Если не удалось - помечаем как отключённого
                client.connected = false;
            }
        }
    }

    bool send_to_client(int client_id, const std::string& message) {
        std::lock_guard<std::mutex> lock(clients_mutex_);

        auto it = clients_.find(client_id);
        if (it == clients_.end()) return false;

        Client& client = it->second;
        if (!client.connected) return false;

        return net_utils::send_message(client.socket, message);
    }
};

ClientManager client_manager;

void handle_client_command(int client_id, const std::string& command) {

    // Команда смены имени: /name НовоеИмя
    if (command.rfind("/name ", 0) == 0) {
        std::string new_name = command.substr(6);
        std::string old_name = client_manager.get_client_name(client_id);
        client_manager.set_client_name(client_id, new_name);

        std::string msg = old_name + " changed name to " + new_name;
        client_manager.broadcast_message(msg);
    }
    // Команда личного сообщения: /msg id сообщение
    else if (command.rfind("/msg ", 0) == 0) {
        size_t space_pos = command.find(' ', 5);
        if (space_pos != std::string::npos) {
            std::string target_id_str = command.substr(5, space_pos - 5);
            std::string private_msg = command.substr(space_pos + 1);

            try {
                int target_id = std::stoi(target_id_str);
                std::string full_msg = "[Personally from " + client_manager.get_client_name(client_id) + "]: " + private_msg;
                client_manager.send_to_client(target_id, full_msg);
                client_manager.send_to_client(client_id,
                    "Message sent to user " + target_id_str);
            }
            catch (...) {
                client_manager.send_to_client(client_id,
                    "Wrong user ID: " + target_id_str + " not found");
            }
        }
    }
    // Команда списка пользователей
    else if (command == "/users") {
        std::string user_list = "Connected users:\n";
        auto connected_clients = client_manager.get_connected_clients();
        for (int id : connected_clients) {
                user_list += "ID: " + std::to_string(id) +
                    " - " + client_manager.get_client_name(id) + "\n";
        }
        client_manager.send_to_client(client_id, user_list);
    }
    // Команда помощи
    else if (command == "/help") {
        std::string help =
            "Availible commands:\n"
            "/name 'NewName' - changes your name\n"
            "/msg 'ID' 'Message' - personal message\n"
            "/users - user list\n"
            "/help - this text\n"
            "/exit - exit";
        client_manager.send_to_client(client_id, help);
    }
}

void handle_client(int client_id, net_utils::socket_t client_socket, struct sockaddr_in client_addr) {
    // Получаем IP клиента для логов
    char client_ip[INET_ADDRSTRLEN];
    #ifdef NET_WINDOWS
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    #else
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    #endif

    std::cout << "Client connected: " << client_ip
        << ":" << client_manager.get_client_name(client_id)
        << " (ID: " << std::to_string(client_id) << ")" << std::endl;

    // Отправляем приветствие
    std::string welcome =
        "Welcome in chat!\n"
        "Your ID: " + std::to_string(client_id) + "\n"
        "Enter /help for command list";
    net_utils::send_message(client_socket, welcome);

    // Сообщаем всем о новом пользователе
    std::string join_msg = "User " + client_manager.get_client_name(client_id) +
        " connected to chat";
    client_manager.broadcast_message(join_msg, client_id);

    // Главный цикл обработки сообщений
    while (true) {
        std::string message = net_utils::read_message(client_socket);

        // Если сообщение пустое - клиент отключился
        if (message.empty()) {
            break;
        }

        // Логируем в консоль сервера
        std::cout << "[" << client_id << "] " << message << std::endl;

        // Проверяем команду
        if (message[0] == '/') {
            handle_client_command(client_id, message);
        }
        else {
            // Обычное сообщение - рассылаем всем
            std::string formatted_msg = "[" + client_manager.get_client_name(client_id) +
                "] " + message;
            client_manager.broadcast_message(formatted_msg, client_id);
        }

        // Проверяем на выход
        if (message == "/exit") {
            break;
        }
    }

    client_manager.disconnect_client(client_id);

    // Сообщаем всем об отключении
    std::string leave_msg = "User " + client_manager.get_client_name(client_id) +
        " left chat";
    client_manager.broadcast_message(leave_msg);

    // Закрываем сокет
    net_utils::socket_close(client_socket);

    std::cout << "Client disconnected: ID " << client_id << std::endl;
}

net_utils::socket_t startListening(int port = 12345) {
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

    // Инициализация сетевой подсистемы (для Windows обязательно)
    if (!net_utils::net_init()) {
        std::cerr << "Network init failed!" << std::endl;
        return 1;
    }

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
    serverAddr.sin_port = htons(port);      // Порт 12345

    // Привязка сокета к адресу
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR_VAL) {
        std::cerr << "Bind failed: " << net_utils::get_last_error() << std::endl;
        net_utils::socket_close(serverSocket);
        net_utils::net_cleanup();
        return 1;
    }

    // Начинаем слушать подключения
    if (listen(serverSocket, 10) == SOCKET_ERROR_VAL) {
        std::cerr << "Error listen: " << net_utils::get_last_error() << std::endl;
        net_utils::socket_close(serverSocket);
        net_utils::net_cleanup();
        return 1;
    }

    std::cout << "Server started. Waiting for connection to port 12345..." << std::endl;

    return serverSocket;
}

int runServer() {

    net_utils::socket_t serverSocket = startListening(12345);

    std::vector<std::thread> client_threads;

    while (true) {

        struct sockaddr_in client_addr;
        #ifdef NET_WINDOWS
        int client_len = sizeof(client_addr);
        #else
        socklen_t client_len = sizeof(client_addr);
        #endif

        net_utils::socket_t client_socket = accept(serverSocket,
            (struct sockaddr*)&client_addr,
            &client_len);

        if (client_socket == net_utils::INVALID_SOCKET_VAL) {
            std::cerr << "Error accept: " << net_utils::get_last_error() << std::endl;
            continue;
        }

        // Добавляем клиента в менеджер
        int client_id = client_manager.add_client(client_socket, client_addr);

        // Запускаем поток для обработки клиента
        client_threads.emplace_back(handle_client, client_id, client_socket, client_addr);

        // Отсоединяем поток (он завершится сам)
        client_threads.back().detach();

        std::cout << "Total clients: " << client_manager.get_client_count() << std::endl;
    }
    net_utils::socket_close(serverSocket);

    net_utils::net_cleanup();  // Для Windows важно!

    std::cout << "Press Enter to exit...";
    std::cin.get();

    return 0;
}
