#include "controller.h"
#include "byteswap.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <WinSock2.h>
#include <ws2bth.h>

#define RESULT_QUEUE_SIZE 64
#define DEFAULT_MUTEX_WAIT_TIME_MS 16
#define RESULT_DATA_BUF_SIZE 512

typedef struct
{
    size_t front, rear, count;
    HANDLE mutex;
    Result data[RESULT_QUEUE_SIZE];
} ResultQueue;

static SOCKET s_Socket;
static HANDLE s_ReceiverThread;
static ResultQueue s_ResultQueue;
static bool s_IsRunning;

static void LockQueueMutex(void)
{
    DWORD result;
    do
    {
        result = WaitForSingleObject(s_ResultQueue.mutex, DEFAULT_MUTEX_WAIT_TIME_MS);
    } while (result != WAIT_OBJECT_0);
}

static void UnlockQueueMutex(void)
{
    ReleaseMutex(s_ResultQueue.mutex);
}

static void EnqueuePacket(Result packet)
{
    if (s_ResultQueue.count >= RESULT_QUEUE_SIZE)
        return;

    LockQueueMutex();
    s_ResultQueue.rear = (s_ResultQueue.rear + 1) % RESULT_QUEUE_SIZE;
    s_ResultQueue.count++;
    s_ResultQueue.data[s_ResultQueue.rear] = packet;

    UnlockQueueMutex();
}

static bool DequeuePacket(Result *outItem)
{
    if (s_ResultQueue.count == 0)
        return false;

    LockQueueMutex();
    Result *item = &s_ResultQueue.data[s_ResultQueue.front];

    s_ResultQueue.front = (s_ResultQueue.front + 1) % RESULT_QUEUE_SIZE;
    s_ResultQueue.count--;
    UnlockQueueMutex();

    memcpy(outItem, item, sizeof(Result));

    return item;
}

static void ByteswapCommand(Command *command)
{
    command->header.id = ByteswapUInt64(command->header.id);
}

static uint64_t GetRandom64(void)
{
    return (uint64_t)rand() | ((uint64_t)rand() << 32);
}

static const char *GetLastWSAError(void)
{
    static char messageBuffer[64 * 1024];

    const int errorCode = WSAGetLastError();
    const DWORD messageLength = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        messageBuffer,
        sizeof(messageBuffer),
        NULL);
    messageBuffer[messageLength] = '\0';

    return messageBuffer;
}

static DWORD WINAPI ReceiverThreadMain(void *unused)
{
    while (s_IsRunning)
    {
        Result result = {0};
        int bytesReceived = recv(s_Socket, (char *)&result.header, sizeof(ResultHeader), 0);
        if (bytesReceived != sizeof(ResultHeader))
        {
            fprintf(stderr, "Failed to receive result header: %s.\n", GetLastWSAError());
            continue;
        }

        if (result.header.dataSize > 0)
        {
            result.data = malloc(result.header.dataSize);
            bytesReceived = recv(s_Socket, result.data, result.header.dataSize, 0);
            if (bytesReceived != result.header.dataSize)
            {
                fprintf(stderr, "Failed to receive result data: %s.\n", GetLastWSAError());
                continue;
            }
        }

        EnqueuePacket(result);
    }

    return 0;
}

static bool SendCommand(Command *command)
{
    ByteswapCommand(command);

    int bytesSent = send(s_Socket, (const char *)&command->header, sizeof(CommandHeader), 0);
    if (bytesSent != sizeof(CommandHeader))
    {
        fprintf(stderr, "Failed to send command header: %s.\n", GetLastWSAError());
        return false;
    }

    const size_t argsSize = sizeof(uint8_t) * command->header.argsCount;
    bytesSent = send(s_Socket, (const char *)command->args, argsSize, 0);
    if (bytesSent != argsSize)
    {
        fprintf(stderr, "Failed to send command args: %s.\n", GetLastWSAError());
        return false;
    }

    return true;
}

bool Controller_Initialize(void)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData))
    {
        fprintf(stderr, "Failed to initialize WSA: %s.\n", GetLastWSAError());
        return false;
    }

    return true;
}

void Controller_Shutdown(void)
{
    if (s_Socket)
        Controller_DisconnectDevice();

    s_IsRunning = false;

    if (s_ReceiverThread)
    {
        WaitForSingleObject(s_ReceiverThread, INFINITE);
        CloseHandle(s_ReceiverThread);
    }

    if (s_ResultQueue.mutex)
        CloseHandle(s_ResultQueue.mutex);

    WSACleanup();

    s_Socket = NULL;
    s_ReceiverThread = NULL;
    s_ResultQueue.mutex = NULL;
    s_IsRunning = false;
}

bool Controller_ConnectDevice(uint64_t deviceAddress, uint32_t port)
{
    s_Socket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (!s_Socket)
    {
        fprintf(stderr, "Failed to create socket: %s.\n", GetLastWSAError());
        return false;
    }

    const SOCKADDR_BTH address = {
        .addressFamily = AF_BTH,
        .port = port,
        .btAddr = deviceAddress,
    };
    if (connect(s_Socket, (const struct sockaddr *)&address, sizeof(SOCKADDR_BTH)))
    {
        fprintf(stderr, "Failed to connect to bluetooth device: %s.", GetLastWSAError());
        return false;
    }

    s_IsRunning = true;
    s_ResultQueue.mutex = CreateMutexA(NULL, FALSE, NULL);
    s_ReceiverThread = CreateThread(
        NULL,
        0,
        ReceiverThreadMain,
        NULL,
        0,
        NULL);

    return true;
}

void Controller_DisconnectDevice(void)
{
    shutdown(s_Socket, SD_BOTH);
    closesocket(s_Socket);

    s_Socket = NULL;
}

bool Controller_GetNextResult(Result *outResult)
{
    return DequeuePacket(outResult);
}

bool Controller_SendCommand(Opcode opcode, uint8_t argsCount, ...)
{
    Command command = {
        .header = {
            .argsCount = argsCount,
            .opcode = opcode,
            .id = GetRandom64(),
        },
    };

    va_list args;
    va_start(args, argsCount);

    for (uint8_t i = 0; i < argsCount; i++)
        command.args[i] = va_arg(args, uint8_t);

    va_end(args);

    return SendCommand(&command);
}

bool Controller_SendCommandArgs(Opcode opcode, uint8_t argsCount, const uint8_t *args)
{
    Command command = {
        .header = {
            .argsCount = argsCount,
            .opcode = opcode,
            .id = GetRandom64(),
        },
    };

    memcpy(command.args, args, sizeof(uint8_t) * argsCount);

    return SendCommand(&command);
}

bool Controller_SendCommandString(Opcode opcode, const char *str)
{
    return Controller_SendCommandArgs(opcode, strlen(str), str);
}
