#pragma once
#include <cstdint>
#include <vector>
#include "data_store.h"

struct MbapHeader {
    uint16_t transactionId;
    uint16_t protocolId;
    uint16_t length;
    uint8_t  unitId;
};

class ModbusServer {
public:
    explicit ModbusServer(DataStore& store);

    // Process a raw Modbus TCP ADU; returns the complete response ADU.
    // Returns an empty vector if the request is too short to be valid.
    std::vector<uint8_t> processRequest(const uint8_t* data, int size);

private:
    DataStore& store_;

    MbapHeader           parseMbap(const uint8_t* data);
    std::vector<uint8_t> buildMbap(const MbapHeader& req, uint16_t pduLen);

    std::vector<uint8_t> handleReadCoils          (const MbapHeader&, const uint8_t* pdu, int pduLen);
    std::vector<uint8_t> handleReadHoldingRegs    (const MbapHeader&, const uint8_t* pdu, int pduLen);
    std::vector<uint8_t> handleReadInputRegs      (const MbapHeader&, const uint8_t* pdu, int pduLen);
    std::vector<uint8_t> handleWriteSingleReg     (const MbapHeader&, const uint8_t* pdu, int pduLen);
    std::vector<uint8_t> handleWriteMultipleRegs  (const MbapHeader&, const uint8_t* pdu, int pduLen);

    std::vector<uint8_t> buildException(const MbapHeader&, uint8_t fc, uint8_t exCode);
};
