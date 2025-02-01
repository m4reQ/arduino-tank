#pragma once
#include <stdint.h>

enum Status : uint8_t
{
    STATUS_OK = 0,
    STATUS_INV_OPCODE = 1,
    STATUS_INV_ARGS_COUNT = 2,
    STATUS_INV_STATE = 3,
    STATUS_NO_HANDLER = 5,
};

enum Opcode : uint8_t
{
    OP_MOVE = 1,
    OP_STOP = 2,
    OP_PRINT_LCD = 3,
    OP_GET_SENSOR_STATE = 4,
    OP_MOVE_HEAD_SENSOR = 5,
    OP_CONFIGURE_LIGHT = 6,
    OP_CONFIGURE_BUZZER = 7,
    OP_RESET = 8,
    OP_AUTORUN = 9,
    _OP_ENUM_MAX,
};

#pragma pack(push, 1)
struct ResultHeader
{
    Status status;
    Opcode servicedOpcode;
    uint64_t servicedID;
    uint64_t dataSize;
};

struct Result
{
    ResultHeader header;
    const void *data;
};

struct CommandHeader
{
    Opcode opcode;
    uint64_t id;
    uint8_t argsCount;
};

struct Command
{
    CommandHeader header;
    uint8_t args[16];
};
#pragma pack(pop)

Result MakeResultData(Status status, const Command *servicedCommand, const void *data, uint64_t dataSize);
Result MakeResult(Status status, const Command *servicedCommand);
Result MakeResultSuccess(const Command *servicedCommand);
