#include "modbus_server.h"
#include <iostream>

// Exception codes
static constexpr uint8_t EX_ILLEGAL_FUNCTION  = 0x01;
static constexpr uint8_t EX_ILLEGAL_ADDRESS   = 0x02;
static constexpr uint8_t EX_ILLEGAL_VALUE     = 0x03;

// Minimum ADU size: 7-byte MBAP + 1-byte FC = 8
static constexpr int MIN_ADU_SIZE = 8;

// ── Helpers ──────────────────────────────────────────────────────────────────

static uint16_t readU16BE(const uint8_t* p) {
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

static void writeU16BE(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v >> 8);
    p[1] = static_cast<uint8_t>(v & 0xFF);
}

// ── ModbusServer ─────────────────────────────────────────────────────────────

ModbusServer::ModbusServer(DataStore& store) : store_(store) {}

MbapHeader ModbusServer::parseMbap(const uint8_t* data) {
    MbapHeader h;
    h.transactionId = readU16BE(data + 0);
    h.protocolId    = readU16BE(data + 2);
    h.length        = readU16BE(data + 4);
    h.unitId        = data[6];
    return h;
}

std::vector<uint8_t> ModbusServer::buildMbap(const MbapHeader& req, uint16_t pduLen) {
    // length field = unitId (1) + PDU
    uint16_t lengthField = 1 + pduLen;
    std::vector<uint8_t> mbap(7);
    writeU16BE(mbap.data() + 0, req.transactionId);
    writeU16BE(mbap.data() + 2, 0x0000);          // protocol id
    writeU16BE(mbap.data() + 4, lengthField);
    mbap[6] = req.unitId;
    return mbap;
}

std::vector<uint8_t> ModbusServer::buildException(const MbapHeader& req, uint8_t fc, uint8_t exCode) {
    std::vector<uint8_t> pdu = { static_cast<uint8_t>(fc | 0x80), exCode };
    auto mbap = buildMbap(req, static_cast<uint16_t>(pdu.size()));
    mbap.insert(mbap.end(), pdu.begin(), pdu.end());
    return mbap;
}

std::vector<uint8_t> ModbusServer::processRequest(const uint8_t* data, int size) {
    if (size < MIN_ADU_SIZE) {
        std::cerr << "Request too short (" << size << " bytes), ignoring\n";
        return {};
    }

    MbapHeader hdr  = parseMbap(data);
    const uint8_t* pdu    = data + 7;          // start of PDU (FC + data)
    int            pduLen = size - 7;

    if (hdr.protocolId != 0x0000) {
        std::cerr << "Unknown protocol ID " << hdr.protocolId << ", ignoring\n";
        return {};
    }

    uint8_t fc = pdu[0];
    std::cout << "FC=0x" << std::hex << static_cast<int>(fc)
              << " TID=" << std::dec << hdr.transactionId << "\n";

    switch (fc) {
        case 0x01: return handleReadCoils         (hdr, pdu, pduLen);
        case 0x03: return handleReadHoldingRegs   (hdr, pdu, pduLen);
        case 0x04: return handleReadInputRegs     (hdr, pdu, pduLen);
        case 0x06: return handleWriteSingleReg    (hdr, pdu, pduLen);
        case 0x10: return handleWriteMultipleRegs (hdr, pdu, pduLen);
        default:
            std::cerr << "Unsupported function code 0x" << std::hex << static_cast<int>(fc) << "\n";
            return buildException(hdr, fc, EX_ILLEGAL_FUNCTION);
    }
}

// ── 0x01 Read Coils ──────────────────────────────────────────────────────────

std::vector<uint8_t> ModbusServer::handleReadCoils(
    const MbapHeader& hdr, const uint8_t* pdu, int pduLen)
{
    if (pduLen < 5)
        return buildException(hdr, 0x01, EX_ILLEGAL_VALUE);

    uint16_t startAddr = readU16BE(pdu + 1);
    uint16_t quantity  = readU16BE(pdu + 3);

    if (quantity == 0 || quantity > 2000)
        return buildException(hdr, 0x01, EX_ILLEGAL_VALUE);
    if (startAddr + quantity > DataStore::MAX_COILS)
        return buildException(hdr, 0x01, EX_ILLEGAL_ADDRESS);

    uint8_t byteCount = static_cast<uint8_t>((quantity + 7) / 8);
    std::vector<uint8_t> coilBytes(byteCount, 0);
    for (uint16_t i = 0; i < quantity; ++i) {
        if (store_.coils[startAddr + i])
            coilBytes[i / 8] |= static_cast<uint8_t>(1 << (i % 8));
    }

    // PDU: FC(1) + byteCount(1) + coilBytes(N)
    std::vector<uint8_t> pduResp = { 0x01, byteCount };
    pduResp.insert(pduResp.end(), coilBytes.begin(), coilBytes.end());

    auto mbap = buildMbap(hdr, static_cast<uint16_t>(pduResp.size()));
    mbap.insert(mbap.end(), pduResp.begin(), pduResp.end());
    return mbap;
}

// ── 0x03 Read Holding Registers ──────────────────────────────────────────────

std::vector<uint8_t> ModbusServer::handleReadHoldingRegs(
    const MbapHeader& hdr, const uint8_t* pdu, int pduLen)
{
    if (pduLen < 5)
        return buildException(hdr, 0x03, EX_ILLEGAL_VALUE);

    uint16_t startAddr = readU16BE(pdu + 1);
    uint16_t quantity  = readU16BE(pdu + 3);

    if (quantity == 0 || quantity > 125)
        return buildException(hdr, 0x03, EX_ILLEGAL_VALUE);
    if (startAddr + quantity > DataStore::MAX_HOLDING_REGS)
        return buildException(hdr, 0x03, EX_ILLEGAL_ADDRESS);

    uint8_t byteCount = static_cast<uint8_t>(quantity * 2);
    std::vector<uint8_t> pduResp = { 0x03, byteCount };
    for (uint16_t i = 0; i < quantity; ++i) {
        uint16_t val = store_.holdingRegisters[startAddr + i];
        pduResp.push_back(static_cast<uint8_t>(val >> 8));
        pduResp.push_back(static_cast<uint8_t>(val & 0xFF));
    }

    auto mbap = buildMbap(hdr, static_cast<uint16_t>(pduResp.size()));
    mbap.insert(mbap.end(), pduResp.begin(), pduResp.end());
    return mbap;
}

// ── 0x04 Read Input Registers ────────────────────────────────────────────────

std::vector<uint8_t> ModbusServer::handleReadInputRegs(
    const MbapHeader& hdr, const uint8_t* pdu, int pduLen)
{
    if (pduLen < 5)
        return buildException(hdr, 0x04, EX_ILLEGAL_VALUE);

    uint16_t startAddr = readU16BE(pdu + 1);
    uint16_t quantity  = readU16BE(pdu + 3);

    if (quantity == 0 || quantity > 125)
        return buildException(hdr, 0x04, EX_ILLEGAL_VALUE);
    if (startAddr + quantity > DataStore::MAX_INPUT_REGS)
        return buildException(hdr, 0x04, EX_ILLEGAL_ADDRESS);

    uint8_t byteCount = static_cast<uint8_t>(quantity * 2);
    std::vector<uint8_t> pduResp = { 0x04, byteCount };
    for (uint16_t i = 0; i < quantity; ++i) {
        uint16_t val = store_.inputRegisters[startAddr + i];
        pduResp.push_back(static_cast<uint8_t>(val >> 8));
        pduResp.push_back(static_cast<uint8_t>(val & 0xFF));
    }

    auto mbap = buildMbap(hdr, static_cast<uint16_t>(pduResp.size()));
    mbap.insert(mbap.end(), pduResp.begin(), pduResp.end());
    return mbap;
}

// ── 0x06 Write Single Register ───────────────────────────────────────────────

std::vector<uint8_t> ModbusServer::handleWriteSingleReg(
    const MbapHeader& hdr, const uint8_t* pdu, int pduLen)
{
    if (pduLen < 5)
        return buildException(hdr, 0x06, EX_ILLEGAL_VALUE);

    uint16_t addr = readU16BE(pdu + 1);
    uint16_t val  = readU16BE(pdu + 3);

    if (addr >= DataStore::MAX_HOLDING_REGS)
        return buildException(hdr, 0x06, EX_ILLEGAL_ADDRESS);

    store_.holdingRegisters[addr] = val;
    std::cout << "HR[" << addr << "] = " << val << "\n";

    // Response: echo the request PDU
    std::vector<uint8_t> pduResp = { 0x06, pdu[1], pdu[2], pdu[3], pdu[4] };
    auto mbap = buildMbap(hdr, static_cast<uint16_t>(pduResp.size()));
    mbap.insert(mbap.end(), pduResp.begin(), pduResp.end());
    return mbap;
}

// ── 0x10 Write Multiple Registers ────────────────────────────────────────────

std::vector<uint8_t> ModbusServer::handleWriteMultipleRegs(
    const MbapHeader& hdr, const uint8_t* pdu, int pduLen)
{
    // Minimum: FC(1)+addr(2)+qty(2)+byteCount(1) = 7, plus at least 2 data bytes
    if (pduLen < 7)
        return buildException(hdr, 0x10, EX_ILLEGAL_VALUE);

    uint16_t startAddr = readU16BE(pdu + 1);
    uint16_t quantity  = readU16BE(pdu + 3);
    uint8_t  byteCount = pdu[5];

    if (quantity == 0 || quantity > 123)
        return buildException(hdr, 0x10, EX_ILLEGAL_VALUE);
    if (byteCount != quantity * 2)
        return buildException(hdr, 0x10, EX_ILLEGAL_VALUE);
    if (pduLen < 6 + byteCount)
        return buildException(hdr, 0x10, EX_ILLEGAL_VALUE);
    if (startAddr + quantity > DataStore::MAX_HOLDING_REGS)
        return buildException(hdr, 0x10, EX_ILLEGAL_ADDRESS);

    for (uint16_t i = 0; i < quantity; ++i) {
        store_.holdingRegisters[startAddr + i] = readU16BE(pdu + 6 + i * 2);
    }
    std::cout << "Wrote " << quantity << " HR(s) starting at " << startAddr << "\n";

    // Response: FC(1) + startAddr(2) + quantity(2)
    std::vector<uint8_t> pduResp = { 0x10, pdu[1], pdu[2], pdu[3], pdu[4] };
    auto mbap = buildMbap(hdr, static_cast<uint16_t>(pduResp.size()));
    mbap.insert(mbap.end(), pduResp.begin(), pduResp.end());
    return mbap;
}
