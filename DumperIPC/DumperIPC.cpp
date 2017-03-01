#include "DumperIPC.hpp"
#include <atomic>
#include <thread>

/// Shared region
#pragma data_seg("SHARED")

// The process we are sending our data to
std::atomic<std::uint32_t> ClientProcess(0);

// The target UWP process we wish to dump
std::atomic<std::uint32_t> TargetProcess(0);

struct StringEntry
{
	constexpr static std::size_t StringSize = 1024;
	bool Active;
	char String[StringSize];
};

std::atomic_flag StringPoolMutex = ATOMIC_FLAG_INIT;
constexpr static std::size_t PoolSize = 1024;
StringEntry StringStack[PoolSize] = { {false,0} };

#pragma data_seg()
#pragma comment(linker, "/section:SHARED,RWS")
///

namespace IPC
{
void SetClientProcess(std::uint32_t ProcessID)
{
	ClientProcess = ProcessID;
}

std::uint32_t GetClientProcess()
{
	return ClientProcess;
}

void SetTargetProcess(std::uint32_t ProcessID)
{
	TargetProcess = ProcessID;
}

std::uint32_t GetTargetProcess()
{
	return TargetProcess;
}

void PushMessage(const char* Message)
{
	while( StringPoolMutex.test_and_set(std::memory_order_acquire) )
	{
		std::this_thread::yield();
	}

	std::size_t i = 0;
	while( StringStack[i++].Active == true && (i < PoolSize) ) {}

	StringStack[i].Active = true;
	strcpy_s(StringStack[i].String, StringEntry::StringSize, Message);

	StringPoolMutex.clear(std::memory_order_release);
}

const char* PopMessage()
{
	const char* Message = nullptr;
	while( StringPoolMutex.test_and_set(std::memory_order_acquire) )
	{
		std::this_thread::yield();
	}

	std::size_t i = 0;
	while( StringStack[i++].Active == true ) {}

	if( i < PoolSize )
	{
		Message = StringStack[i].String;
		StringStack[i].Active = false;
	}

	StringPoolMutex.clear(std::memory_order_release);
	return Message;
}
}