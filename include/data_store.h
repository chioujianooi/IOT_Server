#pragma once
#include <cstdint>
#include <string>

class DataStore {
public:
    static constexpr uint16_t MAX_COILS        = 2000;
    static constexpr uint16_t MAX_HOLDING_REGS = 125;
    static constexpr uint16_t MAX_INPUT_REGS   = 125;

    bool     coils[MAX_COILS]{};
    uint16_t holdingRegisters[MAX_HOLDING_REGS]{};
    uint16_t inputRegisters[MAX_INPUT_REGS]{};

    bool loadFromFile(const std::string& path);
};
