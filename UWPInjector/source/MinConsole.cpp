#include <MinConsole.hpp>
#include <Windows.h>
#include <mutex>

namespace MinConsole
{
void* GetOutputHandle()
{
	static void* Handle = nullptr;
	std::once_flag HandleCached;
	std::call_once(HandleCached, []() noexcept -> void
		{
			Handle = GetStdHandle(STD_OUTPUT_HANDLE);
		});
	return Handle;
}

void SetTextColor(Color NewColor)
{
	SetConsoleTextAttribute(
		GetOutputHandle(),
		static_cast<std::underlying_type_t<MinConsole::Color>>(NewColor)
	);
}

std::size_t GetWidth()
{
	CONSOLE_SCREEN_BUFFER_INFO ConsoleInfo;
	if( GetConsoleScreenBufferInfo(
		GetOutputHandle(),
		&ConsoleInfo) )
	{
		return static_cast<size_t>(ConsoleInfo.dwSize.X);
	}
	return 0;
}
}
