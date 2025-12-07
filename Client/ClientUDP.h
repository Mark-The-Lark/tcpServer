#pragma once
#include "../Common/net_utils.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <iomanip>


class UdpRadioClient {
private:
    net_utils::socket_t broadcast_socket_;     // Для прослушивания трансляции
    net_utils::socket_t response_socket_;              // Для получения ответов
    net_utils::socket_t command_socket_;    // Для отправки команд
    std::atomic<bool> running_{ true };
    std::thread broadcast_thread_;
    std::thread response_thread_;
    std::thread input_thread_;

    const std::string SERVER_IP;
    const int BROADCAST_PORT = 12345;
    const int COMMAND_PORT = 12346;
    int response_port_;

    // Статистика
    std::atomic<int> received_broadcasts_{ 0 };
    std::atomic<int> received_responses_{ 0 };
    std::atomic<int> sent_commands_{ 0 };
public:

    UdpRadioClient(const std::string& server_ip = "127.0.0.1");
    ~UdpRadioClient();

    void start();
    void stop();

private:
    void broadcast_listen_loop();
    void response_listen_loop();
    void input_loop();
    void send_command(const std::string& command);
};
