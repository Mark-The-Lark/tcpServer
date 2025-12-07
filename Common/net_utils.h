#pragma once

// === 1. Определение платформы ===
#if defined(_WIN32) || defined(_WIN64)
#define NET_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#define NET_LINUX
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#endif

#include <iostream>
#include <string>
#include <cstring>
#include <cstdint>

namespace net_utils {
// === 2. Типы и константы ===
    #ifdef NET_WINDOWS
    using socket_t = SOCKET;
    const socket_t INVALID_SOCKET_VAL = INVALID_SOCKET;
    #define SOCKET_ERROR_VAL SOCKET_ERROR
    #else
    using socket_t = int;
    const socket_t INVALID_SOCKET_VAL = -1;
    #define SOCKET_ERROR_VAL -1
    #endif

// === 3. Инициализация/очистка ===
    inline bool net_init() {
        #ifdef NET_WINDOWS
        WSADATA wsaData;
        return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
        #else
        return true;  // В Linux ничего инициализировать не нужно
        #endif
    }

    inline void net_cleanup() {
        #ifdef NET_WINDOWS
        WSACleanup();
        #endif
    }

    inline void socket_close(socket_t sock) {
        #ifdef NET_WINDOWS
        closesocket(sock);
        #else
        close(sock);
        #endif
    }

    inline int get_last_error() {
        #ifdef NET_WINDOWS
        return WSAGetLastError();
        #else
        return errno;
        #endif
    }

    inline socket_t create_tcp_socket() {
        return socket(AF_INET, SOCK_STREAM, 0);
    }

    inline bool SOCKset_timeout(socket_t sock, int milliseconds) {
        #ifdef NET_WINDOWS
        DWORD timeout = milliseconds;
        return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
            (const char*)&timeout, sizeof(timeout)) == 0;
        #else
        struct timeval tv;
        tv.tv_sec = milliseconds / 1000;
        tv.tv_usec = (milliseconds % 1000) * 1000;
        return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
            &tv, sizeof(tv)) == 0;
        #endif
    }

    inline socket_t create_udp_socket() {
        return socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }

    inline bool enable_broadcast(socket_t sock) {
        int broadcast = 1;
        return setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
            (const char*)&broadcast, sizeof(broadcast)) == 0;
    }

    inline bool send_udp(socket_t sock, const char* data, size_t size,
        const char* ip, int port) {
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);


        if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) return false;


        #ifdef NET_WINDOWS
        int sent = sendto(sock, data, (int)size, 0,
            (sockaddr*)&addr, sizeof(addr));
        #else
        ssize_t sent = sendto(sock, data, size, 0,
            (sockaddr*)&addr, sizeof(addr));
        #endif

        return sent > 0;
    }

    inline bool send_udp_string(socket_t sock, const std::string& message,
        const std::string& ip, int port) {
        return send_udp(sock, message.c_str(), message.length(), ip.c_str(), port);
    }

    inline bool send_broadcast(socket_t sock, const std::string& message, int port) {
        return send_udp_string(sock, message, "255.255.255.255", port);
    }

    struct UdpPacket {
        std::string data;
        std::string sender_ip;
        int sender_port;
    };

    inline UdpPacket receive_udp(socket_t sock, int timeout_ms = 0) {
        UdpPacket packet;

        if (timeout_ms > 0) {
            SOCKset_timeout(sock, timeout_ms);
        }

        char buffer[65507]; // Максимальный размер UDP пакета
        sockaddr_in from_addr;

        #ifdef NET_WINDOWS
        int from_len = sizeof(from_addr);
        #else
        socklen_t from_len = sizeof(from_addr);
        #endif

        #ifdef NET_WINDOWS
        int received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
            (sockaddr*)&from_addr, &from_len);
        #else
        ssize_t received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
            (sockaddr*)&from_addr, &from_len);
        #endif

        if (received > 0) {
            buffer[received] = '\0';
            packet.data.assign(buffer, received);

            char ip_str[INET_ADDRSTRLEN];
            #ifdef NET_WINDOWS
            inet_ntop(AF_INET, &from_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
            #else
            inet_ntop(AF_INET, &from_addr.sin_addr, ip_str, sizeof(ip_str));
            #endif

            packet.sender_ip = ip_str;
            packet.sender_port = ntohs(from_addr.sin_port);
        }

        return packet;
    }

    inline bool receive_udp_with_timeout(socket_t sock, UdpPacket& packet,
        int timeout_ms = 1000) {
        packet = receive_udp(sock, timeout_ms);
        return !packet.data.empty();
    }

    inline bool bind_socket(socket_t sock, int port) {
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        return bind(sock, (sockaddr*)&addr, sizeof(addr)) == 0;
    }


    inline bool TCPsend(socket_t socket, const std::string& message) {
        int len = message.length();
        #ifdef _WIN32
        send(socket, reinterpret_cast<char*>(&len), sizeof(int), 0);
        return send(socket, message.c_str(), message.length(), 0);
        #else
        write(socket, reinterpret_cast<char*>(&len), sizeof(int));
        return write(socket, message.c_str(), message.length());
        #endif
    }
    
    inline std::string TCPread(socket_t socket) {
        int len = 0;
        std::string message;
        int msg_bytes_read = 0;
        #ifdef _WIN32
        recv(socket, reinterpret_cast<char*>(&len), sizeof(int), 0);
        message.resize(len);
        while (msg_bytes_read < len) {
            int bytes = recv(socket, &message[msg_bytes_read], len - msg_bytes_read, 0);
            msg_bytes_read += bytes;
        }
        #else
        read(socket, reinterpret_cast<char*>(&len), sizeof(int));
        message.resize(len);
        while (msg_bytes_read < len) {
            int bytes = read(socket, &message[msg_bytes_read], len - msg_bytes_read);
            msg_bytes_read += bytes;
        }
        #endif
        return message;;
    }

    inline void TCPshutdown(socket_t socket) {
        #ifdef _WIN32
        shutdown(socket, SD_BOTH);
        #else
        shutdown(socket, SHUT_RDWR);
        #endif
    }
    #define set_timeout SOCKset_timeout
    #define send_message TCPsend
    #define read_message TCPread
    #define shutdown TCPshutdown
}