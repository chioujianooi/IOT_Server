#include "data_store.h"
#include <fstream>
#include <sstream>
#include <iostream>

bool DataStore::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Config file not found: " << path << " — starting with zeroed data\n";
        return false;
    }

    std::string line;
    int lineNum = 0;
    while (std::getline(file, line)) {
        ++lineNum;
        // Strip leading whitespace and skip blank lines / comments
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos || line[start] == '#')
            continue;

        std::istringstream ss(line);
        std::string type, addrStr, valStr;
        if (!std::getline(ss, type, ',') ||
            !std::getline(ss, addrStr, ',') ||
            !std::getline(ss, valStr, ',')) {
            std::cerr << "config.txt line " << lineNum << ": bad format (expected type,addr,value)\n";
            continue;
        }

        int addr = 0, val = 0;
        try {
            addr = std::stoi(addrStr);
            val  = std::stoi(valStr);
        } catch (...) {
            std::cerr << "config.txt line " << lineNum << ": non-integer address or value\n";
            continue;
        }

        if (type == "HR") {
            if (addr < 0 || addr >= MAX_HOLDING_REGS) {
                std::cerr << "config.txt line " << lineNum << ": HR address " << addr << " out of range\n";
                continue;
            }
            holdingRegisters[addr] = static_cast<uint16_t>(val);
        } else if (type == "CO") {
            if (addr < 0 || addr >= MAX_COILS) {
                std::cerr << "config.txt line " << lineNum << ": CO address " << addr << " out of range\n";
                continue;
            }
            coils[addr] = (val != 0);
        } else if (type == "IR") {
            if (addr < 0 || addr >= MAX_INPUT_REGS) {
                std::cerr << "config.txt line " << lineNum << ": IR address " << addr << " out of range\n";
                continue;
            }
            inputRegisters[addr] = static_cast<uint16_t>(val);
        } else {
            std::cerr << "config.txt line " << lineNum << ": unknown type \"" << type << "\" (use HR, IR, or CO)\n";
        }
    }

    std::cout << "Config loaded from " << path << "\n";
    return true;
}
