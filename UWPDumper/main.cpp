#include <stddef.h>
#include <stdint.h>
#include <fstream>
#include <iomanip>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <ShlObj.h>

#define WIDEIFYIMP(x) L##x
#define WIDEIFY(x) WIDEIFYIMP(x)

#define Wrap 50

#include <queue>

#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "UWP.hpp"

#include "DumperIPC.hpp"

uint32_t __stdcall DumperThread(void *DLLHandle)
{
	std::wstring DumpPath = fs::path(UWP::Current::Storage::GetTempStatePath()) / L"DUMP";

	IPC::SetTargetThread(GetCurrentThreadId());

	IPC::PushMessage(L"UWPDumper Build date(%ls : %ls)\n", WIDEIFY(__DATE__), WIDEIFY(__TIME__));
	IPC::PushMessage(L"\t-https://github.com/Wunkolo/UWPDumper\n");
	IPC::PushMessage(L"Publisher:\n\t%s\n", UWP::Current::GetPublisher().c_str());
	IPC::PushMessage(L"Publisher ID:\n\t%s\n", UWP::Current::GetPublisherID().c_str());
	IPC::PushMessage(L"Publisher Path:\n\t%s\n", UWP::Current::Storage::GetPublisherPath().c_str());
	IPC::PushMessage(L"Package Path:\n\t%s\n", UWP::Current::GetPackagePath().c_str());
	IPC::PushMessage(L"Package Name:\n\t%s\n", UWP::Current::GetFullName().c_str());
	IPC::PushMessage(L"Family Name:\n\t%s\n", UWP::Current::GetFamilyName().c_str());

	IPC::PushMessage(L"Dump Path:\n\t%s\n", DumpPath.c_str());

	std::vector<fs::directory_entry> FileList;

	for( auto& Entry : fs::recursive_directory_iterator(".") )
	{
		if( fs::is_regular_file(Entry.path()) )
		{
			FileList.push_back(Entry);
		}
	}

	IPC::PushMessage(L"\tDumping %zu files\n", FileList.size());

	std::size_t i = 0;
	for( const auto& File : FileList )
	{
		const fs::path WritePath = DumpPath + File.path().wstring().substr(1);

		const std::wstring ReadPath = File.path().wstring().substr(1);
		IPC::PushMessage(
			L"%*.*s %*.1f%%\n",
			Wrap, Wrap,
			ReadPath.c_str() + (ReadPath.length() > Wrap ? (ReadPath.length() - Wrap) : 0),
			Wrap,
			(i / static_cast<float>(FileList.size())) * 100
		);

		fs::create_directories(WritePath.parent_path());
		try
		{
			fs::copy(
				File.path(),
				WritePath,
				fs::copy_options::update_existing
			);
		}
		catch( const fs::filesystem_error &e )
		{
			IPC::PushMessage(
				L"Error copying: %s (%s)\n",
				File.path().c_str(),
				e.what()
			);
			return EXIT_FAILURE;
		}
		i++;
	}

	IPC::PushMessage(L"Dump complete!\n\tPath:\n\t%s\n", DumpPath.c_str());
	IPC::ClearTargetThread();

	FreeLibraryAndExitThread(reinterpret_cast<HMODULE>(DLLHandle), 0);

	return EXIT_SUCCESS;
}

int32_t __stdcall DllMain(HINSTANCE hDLL, uint32_t Reason, void *Reserved)
{
	switch( Reason )
	{
	case DLL_PROCESS_ATTACH:
	{
		if( IPC::GetTargetProcess() == GetCurrentProcessId() )
		{
			// We are the target process to be dumped
			CreateThread(
				nullptr,
				0,
				reinterpret_cast<unsigned long(__stdcall*)(void*)>(&DumperThread),
				hDLL,
				0,
				nullptr
			);
		}
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