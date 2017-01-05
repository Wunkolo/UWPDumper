#include <stddef.h>
#include <stdint.h>
#include <fstream>
#include <iomanip>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <ShlObj.h>

#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "UWP.hpp"

uint32_t __stdcall DumperThread(void *DLLHandle)
{
	std::wstring DumpPath = fs::path(UWP::Current::Storage::GetTempStatePath()) / L"DUMP";

	std::wofstream LogFile;
	LogFile.open(
		fs::path(UWP::Current::Storage::GetTempStatePath()) / L"Log.txt",
		std::ios::trunc
	);

	LogFile << "UWPDumper Build date (" << __DATE__ << " : " << __TIME__ << ')' << std::endl;
	LogFile << "\t-https://github.com/Wunkolo/UWPDumper\n";
	LogFile << std::wstring(80, '-') << std::endl;

	LogFile << "Publisher:\n\t" << UWP::Current::GetPublisher() << std::endl;
	LogFile << "Publisher ID:\n\t" << UWP::Current::GetPublisherID() << std::endl;
	LogFile << "Publisher Path:\n\t" << UWP::Current::Storage::GetPublisherPath() << std::endl;

	LogFile << "Package Path:\n\t" << UWP::Current::GetPackagePath() << std::endl;
	LogFile << "Package Name:\n\t" << UWP::Current::GetFullName() << std::endl;
	LogFile << "Family Name:\n\t" << UWP::Current::GetFamilyName() << std::endl;

	LogFile << "Dumping files..." << std::endl;
	LogFile << "Dump path:\n\t" << DumpPath << std::endl;

	try
	{
		fs::copy(
			UWP::Current::GetPackagePath(),
			DumpPath,
			fs::copy_options::recursive | fs::copy_options::update_existing
		);
	}
	catch( fs::filesystem_error &e )
	{
		LogFile << "Error dumping: " << e.what() << std::endl;
		LogFile << "\tOperand 1: " << e.path1() << std::endl;
		LogFile << "\tOperand 2: " << e.path2() << std::endl;
	}
	LogFile << "Dumping complete!" << std::endl;

	FreeLibraryAndExitThread(reinterpret_cast<HMODULE>(DLLHandle), 0);

	return 0;
}

int32_t __stdcall DllMain(HINSTANCE hDLL, uint32_t Reason, void *Reserved)
{
	switch( Reason )
	{
	case DLL_PROCESS_ATTACH:
	{
		CreateThread(
			nullptr,
			0,
			reinterpret_cast<unsigned long(__stdcall*)(void*)>(&DumperThread),
			hDLL,
			0,
			nullptr
		);
	}
	case DLL_PROCESS_DETACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	default:
	{
		return true;
	}
	}

	return false;
}