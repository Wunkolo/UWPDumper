#pragma once
#include <stdint.h>
#include <memory>

namespace MinConsole
{
enum class Color : uint8_t
{
	None = 0,
	Bright = 0x8,
	Red = 0x4,
	Blue = 0x1,
	Green = 0x2,
	Yellow = Red | Green,
	Cyan = Green | Blue,
	Magenta = Red | Blue,
	Normal = Red | Blue | Green,
	// Bitshift colors left 4 bits to get background version
	BackBright = Bright << 4,
	BackRed = Red << 4,
	BackBlue = Blue << 4,
	BackGreen = Green << 4,
	BackYellow = Yellow << 4,
	BackCyan = Cyan << 4,
	BackMagenta = Magenta << 4,
	// Prefabs
	Error = Bright | Red,
	Info = Bright | Yellow,
	Input = Bright | Cyan,
	Suggestion = Cyan
};

typedef std::underlying_type<Color>::type ColorType;

inline Color operator|(Color Left, Color Right)
{
	return static_cast<Color>(
		static_cast<ColorType>(Left)
		| static_cast<ColorType>(Right)
	);
}

inline Color operator|=(Color Left, Color Right)
{
	Left = Left | Right;
	return Left;
}

inline Color operator^(Color Left, Color Right)
{
	return static_cast<Color>(
		static_cast<ColorType>(Left)
		^ static_cast<ColorType>(Right)
	);
}

inline Color operator^=(Color Left, Color Right)
{
	Left = Left ^ Right;
	return Left;
}

void SetTextColor(Color NewColor);

std::size_t GetWidth();
}
