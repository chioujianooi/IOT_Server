#include "socket.h"
#include "data_store.h"
#include "modbus_server.h"
#include <iostream>
#include <cstdint>

static constexpr int  MODBUS_PORT    = 5020;
static constexpr int  MAX_ADU_SIZE   = 260;   // Modbus TCP max frame size
static constexpr int  MAX_CLIENTS    = 5;
static const char*    LISTEN_IP      = "0.0.0.0";
static const char*    CONFIG_FILE    = "config.txt";

int main() {
    DataStore store;
    store.loadFromFile(CONFIG_FILE);

    ModbusServer modbusServer(store);

    ServerSocket server;
    if (!server.create()) {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }

    std::string ip(LISTEN_IP);
    if (!server.customBind(MODBUS_PORT, ip.c_str(), static_cast<int>(ip.length()))) {
        std::cerr << "Failed to bind on port " << MODBUS_PORT << "\n";
        return 1;
    }

    if (!server.customListen(MAX_CLIENTS)) {
        std::cerr << "Failed to listen\n";
        return 1;
    }

    std::cout << "Modbus TCP server listening on port " << MODBUS_PORT << "\n";

    while (true) {
        auto client = server.customAccept();
        std::cout << "Client connected\n";

        uint8_t buffer[MAX_ADU_SIZE];
        while (true) {
            int received = server.receiveData(reinterpret_cast<char*>(buffer), sizeof(buffer), client);
            if (received <= 0) break;

            auto response = modbusServer.processRequest(buffer, received);
            if (!response.empty()) {
                server.sendData(
                    reinterpret_cast<const char*>(response.data()),
                    static_cast<int>(response.size()),
                    client
                );
            }
        }

        std::cout << "Client disconnected\n";
    }

    return 0;
}
