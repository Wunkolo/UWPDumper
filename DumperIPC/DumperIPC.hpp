#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

#ifdef IPC_EXPORT
#define IPC_API __declspec(dllexport)
#else
#define IPC_API __declspec(dllimport)
#endif

namespace IPC
{
IPC_API void SetClientProcess(std::uint32_t ProcessID);
IPC_API std::uint32_t GetClientProcess();
IPC_API void SetTargetProcess(std::uint32_t ProcessID);
IPC_API std::uint32_t GetTargetProcess();

IPC_API void PushMessage(const char* Message);
IPC_API const char* PopMessage();
}