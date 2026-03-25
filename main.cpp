#include "socket.h"
#include <string>
#include <iostream>

int main() {
    ServerSocket server;
    server.create();
    std::string ip="192.168.56.1";
    server.customBind(8080,ip.c_str(),static_cast<int>(ip.length()));
    server.customListen(5);
    auto client = server.customAccept();

    char buffer[1024];
    server.receiveData(buffer, sizeof(buffer), client);
    std::cout<< "Received: " << buffer << std::endl;

    server.sendData("Hello from server!", 18, client);

    return 0;
}