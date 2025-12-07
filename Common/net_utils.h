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

    inline bool set_timeout(socket_t sock, int milliseconds) {
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

    inline socket_t create_udp_socket() {
        return socket(AF_INET, SOCK_DGRAM, 0);
    }

    inline bool NUsend(socket_t socket, const std::string& message) {
        int len = message.length();
        #ifdef _WIN32
        send(socket, reinterpret_cast<char*>(&len), sizeof(int), 0);
        return send(socket, message.c_str(), message.length(), 0);
        #else
        write(socket, reinterpret_cast<char*>(&len), sizeof(int));
        return write(socket, message.c_str(), message.length());
        #endif
    }
    
    inline std::string NUread(socket_t socket) {
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

    inline bool NUset_timeout(socket_t sock, int milliseconds) {
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

    void NUshutdown(socket_t socket) {
        #ifdef _WIN32
        shutdown(socket, SD_BOTH);
        #else
        shutdown(socket, SHUT_RDWR);
        #endif
    }
    #define set_timeout NUset_timeout
    #define send_message NUsend
    #define read_message NUread
    #define shutdown NUshutdown
}