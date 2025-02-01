#include "byteswap.h"

uint16_t ByteswapUInt16(uint16_t value)
{
    return (value << 8) | (value >> 8);
}

int16_t ByteswapInt16(int16_t value)
{
    return (value << 8) | ((value >> 8) & 0xFF);
}

uint32_t ByteswapUInt32(uint32_t value)
{
    value = ((value << 8) & 0xFF00FF00) | ((value >> 8) & 0xFF00FF);
    return (value << 16) | (value >> 16);
}

int32_t ByteswapInt32(int32_t value)
{
    value = ((value << 8) & 0xFF00FF00) | ((value >> 8) & 0xFF00FF);
    return (value << 16) | ((value >> 16) & 0xFFFF);
}

uint64_t ByteswapUInt64(uint64_t value)
{
    value = ((value << 8) & 0xFF00FF00FF00FF00ULL) | ((value >> 8) & 0x00FF00FF00FF00FFULL);
    value = ((value << 16) & 0xFFFF0000FFFF0000ULL) | ((value >> 16) & 0x0000FFFF0000FFFFULL);
    return (value << 32) | (value >> 32);
}

int64_t ByteswapInt64(int64_t value)
{
    value = ((value << 8) & 0xFF00FF00FF00FF00ULL) | ((value >> 8) & 0x00FF00FF00FF00FFULL);
    value = ((value << 16) & 0xFFFF0000FFFF0000ULL) | ((value >> 16) & 0x0000FFFF0000FFFFULL);
    return (value << 32) | ((value >> 32) & 0xFFFFFFFFULL);
}
