#include <UWP/DumperIPC.hpp>

#include <array>
#include <atomic>
#include <thread>
#include <cstdarg>

/// API
namespace IPC
{
/// IPC Message Queue
struct MessageEntry
{
	MessageEntry()
	{ }

	explicit MessageEntry(const wchar_t* String)
	{
		wcscpy_s(this->String, MessageEntry::StringSize, String);
	}

	static constexpr std::size_t StringSize = 1024;
	wchar_t String[StringSize];
};

template< typename QueueType, std::size_t PoolSize >
class SafeQueue
{
public:
	using Type = QueueType;
	static constexpr std::size_t MaxSize = PoolSize;

	SafeQueue()
		:
		Head(0),
		Tail(0)
	{ }

	~SafeQueue()
	{ }

	void Enqueue(const Type& Entry)
	{
		while( Mutex.test_and_set(std::memory_order_acquire) )
		{
			std::this_thread::yield();
		}
		Entries[Tail] = Entry;
		Tail = (Tail + 1) % MaxSize;
		Mutex.clear(std::memory_order_release);
	}

	Type Dequeue()
	{
		while( Mutex.test_and_set(std::memory_order_acquire) )
		{
			std::this_thread::yield();
		}
		Type Temp = Entries[Head];
		Head = (Head + 1) % MaxSize;
		Mutex.clear(std::memory_order_release);
		return Temp;
	}

	std::size_t Size()
	{
		while( Mutex.test_and_set(std::memory_order_acquire) )
		{
			std::this_thread::yield();
		}
		Mutex.clear(std::memory_order_release);
		const std::size_t Result = Tail - Head;
		return Result;
	}

	bool Empty()
	{
		return Size() == 0;
	}

private:
	std::array<Type, MaxSize> Entries = {
		Type()
	};
	std::size_t Head = 0;
	std::size_t Tail = 0;
	std::atomic_flag Mutex = ATOMIC_FLAG_INIT;
};

////// Shared IPC Region //////////////////////////////////////////////////////
#pragma data_seg("SHARED")
SafeQueue<MessageEntry, 1024> MessagePool = {};
std::atomic<std::size_t> CurMessageCount = 0;

// The process we are sending our data to
std::atomic<std::uint32_t> ClientProcess(0);

// The target UWP process we wish to dump
std::atomic<std::uint32_t> TargetProcess(0);

std::atomic<std::int32_t> TargetThread(InvalidThread);

#pragma data_seg()
#pragma comment(linker, "/section:SHARED,RWS")
///////////////////////////////////////////////////////////////////////////////

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

IPC_API void SetTargetThread(std::int32_t ThreadID)
{
	TargetThread = ThreadID;
}

IPC_API std::int32_t GetTargetThread()
{
	return TargetThread;
}

IPC_API void ClearTargetThread()
{
	TargetThread = InvalidThread;
}

void PushMessage(const wchar_t* Format, ...)
{
	std::va_list Args;
	MessageEntry Entry;

	va_start(Args, Format);
	vswprintf_s(
		Entry.String,
		Entry.StringSize,
		Format,
		Args
	);
	va_end(Args);

	MessagePool.Enqueue(Entry);
}

std::wstring PopMessage()
{
	const MessageEntry Entry = MessagePool.Dequeue();
	return std::wstring(
		Entry.String,
		wcslen(Entry.String)
	);
}

std::size_t MessageCount()
{
	return MessagePool.Size();
}
}
