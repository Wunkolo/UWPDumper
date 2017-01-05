#include <stddef.h>
#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <ShlObj.h>

#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "UWP.hpp"

uint32_t __stdcall DumperThread(void*)
{
	std::wstring DumpPath = fs::path(UWP::Current::Storage::GetTempStatePath()) / L"DUMP";

	//LOG << "Dumping files..." << std::endl;
	//LOG << "Dump path: " << DumpPath << std::endl;

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
		//LOG << "Error dumping: " << e.what() << std::endl;
		//LOG << "\tOperand 1: " << e.path1() << std::endl;
		//LOG << "\tOperand 2: " << e.path2() << std::endl;
		//return false;
	}
	//LOG << "Dumping complete!" << std::endl;

	return 0;
}

int32_t __stdcall DllMain(HINSTANCE hDLL, uint32_t Reason, void *Reserved)
{
	switch( Reason )
	{
	case DLL_PROCESS_ATTACH:
	{
		if( !DisableThreadLibraryCalls(hDLL) )
		{
			return false;
		}

		CreateThread(
			nullptr,
			0,
			reinterpret_cast<unsigned long(__stdcall*)(void*)>(&DumperThread),
			nullptr,
			0,
			nullptr);
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