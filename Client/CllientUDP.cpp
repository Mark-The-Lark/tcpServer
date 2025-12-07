#include "ClientUDP.h"

UdpRadioClient::UdpRadioClient(const std::string& server_ip)
        : SERVER_IP(server_ip), response_port_(0) {

        if (!net_utils::net_init()) {
            throw std::runtime_error("Network init failed");
        }

        //1. Создаём сокет для прослушивания трансляции
        broadcast_socket_ = net_utils::create_udp_socket();
        if (broadcast_socket_ == net_utils::INVALID_SOCKET_VAL) {
            throw std::runtime_error("Listen socket creation failed");
        }

        // Биндим на broadcast порт
        if (!net_utils::bind_socket(broadcast_socket_, BROADCAST_PORT)) {
            net_utils::socket_close(broadcast_socket_);
            throw std::runtime_error("Bind failed");
        }

        // 2. Сокет для получения ответов от сервера
        response_socket_ = net_utils::create_udp_socket();
        if (response_socket_ == net_utils::INVALID_SOCKET_VAL) {
            net_utils::socket_close(broadcast_socket_);
            throw std::runtime_error("Response socket creation failed");
        }

        // Биндим на случайный порт (0 = система выберет свободный)
        sockaddr_in response_addr;
        memset(&response_addr, 0, sizeof(response_addr));
        response_addr.sin_family = AF_INET;
        response_addr.sin_addr.s_addr = INADDR_ANY;
        response_addr.sin_port = 0; // Случайный порт

        if (bind(response_socket_, (sockaddr*)&response_addr, sizeof(response_addr)) != 0) {
            net_utils::socket_close(broadcast_socket_);
            net_utils::socket_close(response_socket_);
            throw std::runtime_error("Response bind failed");
        }

        // Узнаём какой порт выбрала система
        socklen_t addr_len = sizeof(response_addr);
        getsockname(response_socket_, (sockaddr*)&response_addr, &addr_len);
        response_port_ = ntohs(response_addr.sin_port);

        //3. Создаём сокет для отправки команд
        command_socket_ = net_utils::create_udp_socket();
        if (command_socket_ == net_utils::INVALID_SOCKET_VAL) {
            net_utils::socket_close(broadcast_socket_);
            throw std::runtime_error("Command socket creation failed");
        }

        std::cout << "UDP Radio Client started" << std::endl;
        std::cout << "Listening broadcast on port: " << BROADCAST_PORT << std::endl;
        std::cout << "Listening responses on port: " << response_port_ << std::endl;
        std::cout << "Sending commands to: " << SERVER_IP << ":" << COMMAND_PORT << std::endl;
        std::cout << "Commands: HELLO, STATUS, ECHO <text>, TIME, PING, exit" << std::endl;
    }

UdpRadioClient::~UdpRadioClient() {
        stop();
        net_utils::socket_close(response_socket_);
        net_utils::socket_close(command_socket_);
        net_utils::socket_close(broadcast_socket_);
        net_utils::net_cleanup();
    }

    void UdpRadioClient::start() {
        // Запускаем поток прослушивания
        broadcast_thread_ = std::thread(&UdpRadioClient::broadcast_listen_loop, this);
        // Запускаем поток прослушивания ответов
        response_thread_ = std::thread(&UdpRadioClient::response_listen_loop, this);
        // Запускаем поток ввода команд
        input_thread_ = std::thread(&UdpRadioClient::input_loop, this);

        std::cout << "Client started. Type commands..." << std::endl;

        // Ждём завершения потоков
        broadcast_thread_.join();
        input_thread_.join();
        response_thread_.join();
    }

    void UdpRadioClient::stop() {
        running_ = false;

        // Закрываем сокеты чтобы выйти из блокирующих вызовов
        net_utils::socket_close(broadcast_socket_);
        net_utils::socket_close(response_socket_);
        net_utils::socket_close(command_socket_);

        std::cout << "\nClient stopped." << std::endl;
        std::cout << "Received broadcasts: " << received_broadcasts_ << std::endl;
        std::cout << "Received responses: " << received_responses_ << std::endl;
        std::cout << "Sent commands: " << sent_commands_ << std::endl;
    }

    // Поток прослушивания трансляции
    void UdpRadioClient::broadcast_listen_loop() {
        std::cout << "Listening for broadcasts..." << std::endl;

        while (running_) {
            // Слушаем трансляцию с таймаутом
            net_utils::UdpPacket packet;
            if (net_utils::receive_udp_with_timeout(broadcast_socket_, packet, 100)) {
                received_broadcasts_++;

                // Выводим трансляцию с временной меткой
                auto now = std::chrono::system_clock::now();
                auto time = std::chrono::system_clock::to_time_t(now);

                struct tm time_info;
                localtime_s(&time_info, &time);
                std::cout << "\n[" << std::put_time(&time_info, "%H:%M:%S")
                    << "] BROADCAST: " << packet.data << std::endl;
                std::cout << "> " << std::flush;
            }
        }

        std::cout << "Broadcast listener stopped" << std::endl;
    }

    void UdpRadioClient::response_listen_loop() {
        std::cout << "Response listener started on port " << response_port_ << std::endl;

        while (running_) {
            net_utils::UdpPacket packet;
            if (net_utils::receive_udp_with_timeout(response_socket_, packet, 100)) {
                received_responses_++;

                auto now = std::chrono::system_clock::now();
                auto time = std::chrono::system_clock::to_time_t(now);

                struct tm time_info;
                localtime_s(&time_info, &time);

                std::cout << "\n[" << std::put_time(&time_info, "%H:%M:%S")
                    << "] Response #" << received_responses_
                    << " from " << packet.sender_ip << ":" << packet.sender_port
                    << ": " << packet.data << std::endl;
                std::cout << "> " << std::flush;
            }
        }

        std::cout << "Response listener stopped" << std::endl;
    }

    // Поток ввода команд
    void UdpRadioClient::input_loop() {
        std::string input;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        send_command("HELLO " + std::to_string(response_port_));

        while (running_) {
            std::cout << "> ";
            std::getline(std::cin, input);

            if (!running_) break;
            if (input.empty()) continue;

            // Команда выхода
            if (input == "exit") {
                send_command("GOODBYE " + std::to_string(response_port_));
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                stop();
                break;
            }

            // Отправляем команду
            send_command(input + " " + std::to_string(response_port_));
        }
    }

    // Отправка команды и ожидание ответа
    void UdpRadioClient::send_command(const std::string& command) {
        if (!running_) return;

        net_utils::set_timeout(command_socket_, 1000);

        if (net_utils::send_udp_string(command_socket_, command, SERVER_IP, COMMAND_PORT)) {
            sent_commands_++;
            std::cout << "Command sent: " << command << std::endl;
        }
        else {
            std::cerr << "Failed to send command: " << command << std::endl;
        }
    }
