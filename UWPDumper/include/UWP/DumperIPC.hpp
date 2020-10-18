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
	MessageEntry() = default;
	explicit MessageEntry(const wchar_t* String);

	static constexpr std::size_t StringSize = 1024;
	wchar_t String[StringSize] = { 0 };
};

IPC_API void SetClientProcess(std::uint32_t ProcessID);
IPC_API std::uint32_t GetClientProcess();
IPC_API void SetTargetProcess(std::uint32_t ProcessID);
IPC_API std::uint32_t GetTargetProcess();

constexpr std::int32_t InvalidThread = -1;

IPC_API void SetTargetThread(std::int32_t ThreadID);
IPC_API std::int32_t GetTargetThread();
IPC_API void ClearTargetThread();

// String messaging
IPC_API void PushMessage(const wchar_t* Format, ...);
IPC_API bool PopMessage(std::wstring& Output);
IPC_API std::size_t MessageCount();
}