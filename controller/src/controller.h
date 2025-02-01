#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MAX_COMMAND_ARGS 16

typedef enum
{
    PACKET_TYPE_RESULT = 1,
    PACKET_TYPE_SENSOR_DATA = 2,
    PACKET_TYPE_SCAN_DATA = 3,
} PacketType;

typedef enum
{
    STATUS_SUCCESS = 0,
    STATUS_INV_OPCODE = 1,
    STATUS_INV_ARGS_COUNT = 2,
    STATUS_INV_STATE = 3,
    STATUS_OPERATING = 4,
} Status;

typedef enum
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
} Opcode;

#pragma pack(push, 1)
typedef struct
{
    uint8_t opcode;
    uint64_t id;
    uint8_t argsCount;
} CommandHeader;

typedef struct
{
    CommandHeader header;
    uint8_t args[16];
} Command;

typedef struct
{
    uint8_t status;
    uint8_t servicedOpcode;
    uint64_t servicedID;
    uint64_t dataSize;
} ResultHeader;

typedef struct
{
    ResultHeader header;
    void *data;
} Result;
#pragma pack(pop)

typedef void (*ResultCallback)(Result *result);

bool Controller_Initialize(void);
void Controller_Shutdown(void);
bool Controller_ConnectDevice(uint64_t deviceAddress, uint32_t port);
void Controller_DisconnectDevice(void);
bool Controller_GetNextResult(Result *outResult);
bool Controller_SendCommand(Opcode opcode, uint8_t argsCount, ...);
bool Controller_SendCommandArgs(Opcode opcode, uint8_t argsCount, const uint8_t *args);
bool Controller_SendCommandString(Opcode opcode, const char *str);
