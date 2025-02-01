#include "communication.hpp"

Result MakeResultData(Status status, const Command *servicedCommand, const void *data, uint64_t dataSize)
{
    return Result{
        ResultHeader{
            status,
            servicedCommand->header.opcode,
            servicedCommand->header.id,
            dataSize,
        },
        data,
    };
}

Result MakeResult(Status status, const Command *servicedCommand)
{
    return MakeResultData(status, servicedCommand, nullptr, 0);
}

Result MakeResultSuccess(const Command *servicedCommand)
{
    return MakeResult(STATUS_OK, servicedCommand);
}
