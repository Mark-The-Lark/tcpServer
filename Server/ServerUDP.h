#pragma once
#include "../Common/net_utils.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>
#include <random>

class UdpRadioServer {
private:
    net_utils::socket_t server_socket_;
    std::atomic<bool> running_{ true };
    std::thread broadcast_thread_;
    std::thread receive_thread_;

    struct ClientInfo {
        std::string last_command;
        int response_port;
        std::chrono::system_clock::time_point last_active;
    };

    // Клиенты, которые отправили хоть что-то
    std::map<std::string, ClientInfo> clients_;
    std::mutex clients_mutex_;

    // Статистика
    std::atomic<int> broadcast_count_{ 0 };
    std::atomic<int> received_count_{ 0 };
    std::atomic<int> response_count_{ 0 };

    const int BROADCAST_PORT = 12345;
    const int RESPONSE_PORT = 12346;
public:
    UdpRadioServer();
    ~UdpRadioServer();
    void start();
    void stop();
private:
    std::string generate_broadcast_data();
    void broadcast_loop();
    void receive_loop();
    void process_command(const net_utils::UdpPacket& packet);
    void cleanup_inactive_clients();
    size_t get_client_count();
};
