#include "ServerUDP.h"
#include <sstream>
#include <iomanip>
UdpRadioServer::UdpRadioServer() {
        if (!net_utils::net_init()) {
            throw std::runtime_error("Network init failed");
        }

        // Создаём сокет для приёма команд и ответов
        server_socket_ = net_utils::create_udp_socket();
        if (server_socket_ == net_utils::INVALID_SOCKET_VAL) {
            throw std::runtime_error("Socket creation failed");
        }

        // Биндим на порт для приёма
        if (!net_utils::bind_socket(server_socket_, RESPONSE_PORT)) {
            net_utils::socket_close(server_socket_);
            throw std::runtime_error("Bind failed");
        }

        // Включаем broadcast для трансляции
        if (!net_utils::enable_broadcast(server_socket_)) {
            std::cerr << "Warning: Broadcast not enabled" << std::endl;
        }

        std::cout << "UDP Radio Server started" << std::endl;
        std::cout << "Broadcast port: " << BROADCAST_PORT << std::endl;
        std::cout << "Response port: " << RESPONSE_PORT << std::endl;
    }

UdpRadioServer::~UdpRadioServer() {
        stop();
        net_utils::socket_close(server_socket_);
        net_utils::net_cleanup();
    }

    void UdpRadioServer::start() {
        // Запускаем поток трансляции
        broadcast_thread_ = std::thread(&UdpRadioServer::broadcast_loop, this);

        // Запускаем поток приёма
        receive_thread_ = std::thread(&UdpRadioServer::receive_loop, this);

        std::cout << "Server started. Press Enter to stop..." << std::endl;
        std::cout << "Available commands from clients:" << std::endl;
        std::cout << "  HELLO <port>    - client registration" << std::endl;
        std::cout << "  STATUS <port>   - server statistics" << std::endl;
        std::cout << "  ECHO <text> <port> - echo-test" << std::endl;
        std::cout << "  TIME <port>     - server time" << std::endl;
        std::cout << "  PING <port>     - response test" << std::endl;
        std::cout << "  GOODBYE <port>  - disconnect" << std::endl;

        std::cin.get();

        stop();
    }

    void UdpRadioServer::stop() {
        running_ = false;

        if (broadcast_thread_.joinable()) {
            broadcast_thread_.join();
        }

        if (receive_thread_.joinable()) {
            receive_thread_.join();
        }

        std::cout << "\nServer stopped." << std::endl;
        std::cout << "Broadcast messages: " << broadcast_count_ << std::endl;
        std::cout << "Received commands: " << received_count_ << std::endl;
        std::cout << "Sent responses: " << response_count_ << std::endl;
        std::cout << "Active clients: " << clients_.size() << std::endl;
    }

    // Генерация случайных данных для трансляции
    std::string UdpRadioServer::generate_broadcast_data() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(1000, 9999);

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        std::string data = "[RADIO] Time: ";
        struct tm time_info;
        localtime_s(&time_info, &time);
        std::ostringstream oss;
        oss << std::put_time(&time_info, "%Y-%m-%d %H:%M:%S");
        data += oss.str();
        data.pop_back(); // Убираем \n
        data += " | Data: " + std::to_string(dis(gen));
        data += " | Seq: " + std::to_string(broadcast_count_);
        data += " | Clients: " + std::to_string(get_client_count());

        return data;
    }

    // Поток трансляции (радиовещание)
    void UdpRadioServer::broadcast_loop() {
        std::cout << "Broadcast thread started" << std::endl;

        while (running_) {
            // Генерируем данные для трансляции
            std::string broadcast_data = generate_broadcast_data();

            // Отправляем broadcast всем в сети
            if (net_utils::send_broadcast(server_socket_, broadcast_data, BROADCAST_PORT)) {
                broadcast_count_++;

                // Выводим каждую 10-ю трансляцию
                if (broadcast_count_ % 10 == 0) {
                    std::cout << "Broadcast #" << broadcast_count_
                        << ": " << broadcast_data.substr(0, 40) << "..." << std::endl;
                }
            }

            if (broadcast_count_ % 30 == 0) {
                cleanup_inactive_clients();
            }

            // Ждём 1 секунду между трансляциями
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "Broadcast thread stopped" << std::endl;
    }

    // Поток приёма команд и ответов
    void UdpRadioServer::receive_loop() {
        std::cout << "Receive thread started" << std::endl;

        while (running_) {
            // Ждём входящие датаграммы с таймаутом
            net_utils::UdpPacket packet;
            if (net_utils::receive_udp_with_timeout(server_socket_, packet, 100)) {
                received_count_++;
                process_command(packet);
            }
        }

        std::cout << "Receive thread stopped" << std::endl;
    }

    // Обработка входящих пакетов
    void UdpRadioServer::process_command(const net_utils::UdpPacket& packet) {
        // Сохраняем информацию о клиенте
        std::string command = packet.data;
        int response_port = packet.sender_port; // По умолчанию порт отправителя

        // Ищем порт в конце команды (формат: "COMMAND <port>")
        size_t last_space = command.find_last_of(' ');
        if (last_space != std::string::npos) {
            try {
                response_port = std::stoi(command.substr(last_space + 1));
                command = command.substr(0, last_space);
            }
            catch (...) {
                // Если не число, оставляем порт отправителя
            }
        }

        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            clients_[packet.sender_ip] = {
                command,
                response_port,
                std::chrono::system_clock::now()
            };
        }

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        struct tm time_info;
        localtime_s(&time_info, &time);

        std::cout << "\n[" << std::put_time(&time_info, "%H:%M:%S") << "] "
            << packet.sender_ip << ":" << packet.sender_port
            << " -> " << command
            << " (response port: " << response_port << ")" << std::endl;

        // Обрабатываем команду
        std::string response;

        // Обрабатываем команды
        if (command == "HELLO") {
            response = "WELCOME to UDP Radio Server! Your response port: " +
                std::to_string(response_port) +
                "\nAvailable commands: STATUS, ECHO, TIME, PING, GOODBYE";
        }
        else if (command == "STATUS") {
            response = "SERVER STATUS:\n"
                "  Uptime: " + std::to_string(broadcast_count_) + " seconds\n" +
                "  Broadcasts: " + std::to_string(broadcast_count_) + "\n" +
                "  Commands received: " + std::to_string(received_count_) + "\n" +
                "  Responses sent: " + std::to_string(response_count_) + "\n" +
                "  Active clients: " + std::to_string(get_client_count());
        }
        else if (command.rfind("ECHO ", 0) == 0) {
            std::string echo_text = command.substr(5);
            response = "ECHO: " + echo_text;
        }
        else if (command == "TIME") {
            std::ostringstream oss;
            oss << std::put_time(&time_info, "%Y-%m-%d %H:%M:%S");
            response = "SERVER TIME: ";
            response += oss.str();
        }
        else if (command == "PING") {
            response = "PONG from UDP Radio Server";
        }
        else if (command == "GOODBYE") {
            response = "GOODBYE! Thanks for using UDP Radio";
            // Удаляем клиента
            std::lock_guard<std::mutex> lock(clients_mutex_);
            clients_.erase(packet.sender_ip);
        }
        else {
            response = "UNKNOWN COMMAND: " + command +
                "\nAvailable: HELLO, STATUS, ECHO, TIME, PING, GOODBYE";
        }

        // Отправляем ответ на указанный порт
        if (net_utils::send_udp_string(server_socket_, response, packet.sender_ip.c_str(), response_port)) {
            response_count_++;
            std::cout << "Response sent to " << packet.sender_ip
                << ":" << response_port << std::endl;
        }
        else {
            std::cerr << "Failed to send response to " << packet.sender_ip
                << ":" << response_port << std::endl;
        }
    }

    void UdpRadioServer::cleanup_inactive_clients() {
        std::lock_guard<std::mutex> lock(clients_mutex_);

        auto now = std::chrono::system_clock::now();
        auto threshold = now - std::chrono::seconds(60); // 60 секунд неактивности

        for (auto it = clients_.begin(); it != clients_.end(); ) {
            if (it->second.last_active < threshold) {
                std::cout << "Removing inactive client: " << it->first << std::endl;
                it = clients_.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    size_t UdpRadioServer::get_client_count() {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        return clients_.size();
    }