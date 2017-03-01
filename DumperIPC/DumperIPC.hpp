#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

#ifdef IPC_EXPORT
#define IPC_API __declspec(dllexport)
#else
#define IPC_API __declspec(dllimport)
#endif

/// Exports
namespace IPC
{
struct MessageEntry
{
	MessageEntry();
	MessageEntry(const wchar_t* String);

	static constexpr std::size_t StringSize = 512;
	wchar_t String[StringSize];
};

IPC_API void SetClientProcess(std::uint32_t ProcessID);
IPC_API std::uint32_t GetClientProcess();
IPC_API void SetTargetProcess(std::uint32_t ProcessID);
IPC_API std::uint32_t GetTargetProcess();

// String messaging
IPC_API void PushMessage(const wchar_t* Message);
IPC_API void PushMessage(const MessageEntry& Message);
IPC_API MessageEntry PopMessage();
IPC_API std::size_t MessageCount();
}