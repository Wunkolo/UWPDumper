#include "MinConsole.hpp"
#include <Windows.h>

namespace MinConsole
{
void* GetOutputHandle()
{
	static void* Handle = nullptr;

	if( Handle == nullptr )
	{
		Handle = GetStdHandle(STD_OUTPUT_HANDLE);
	}

	return Handle;
}

void SetTextColor(Color NewColor)
{
	SetConsoleTextAttribute(
		GetOutputHandle(),
		static_cast<std::underlying_type_t<MinConsole::Color>>(NewColor)
	);
}

size_t GetWidth()
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