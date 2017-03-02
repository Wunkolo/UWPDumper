#include <stddef.h>
#include <stdint.h>
#include <fstream>
#include <iomanip>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <ShlObj.h>

#include <queue>

#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "UWP.hpp"

#include "DumperIPC.hpp"

uint32_t __stdcall DumperThread(void *DLLHandle)
{
	std::wstring DumpPath = fs::path(UWP::Current::Storage::GetTempStatePath()) / L"DUMP";

	//std::wofstream LogFile;
	//LogFile.open(
	//	fs::path(UWP::Current::Storage::GetTempStatePath()) / L"Log.txt",
	//	std::ios::trunc
	//);

	//LogFile << "UWPDumper Build date (" << __DATE__ << " : " << __TIME__ << ')' << std::endl;
	//LogFile << "\t-https://github.com/Wunkolo/UWPDumper\n";
	//LogFile << std::wstring(80, '-') << std::endl;

	//LogFile << "Publisher:\n\t" << UWP::Current::GetPublisher() << std::endl;
	//LogFile << "Publisher ID:\n\t" << UWP::Current::GetPublisherID() << std::endl;
	//LogFile << "Publisher Path:\n\t" << UWP::Current::Storage::GetPublisherPath() << std::endl;

	//LogFile << "Package Path:\n\t" << UWP::Current::GetPackagePath() << std::endl;
	//LogFile << "Package Name:\n\t" << UWP::Current::GetFullName() << std::endl;
	//LogFile << "Family Name:\n\t" << UWP::Current::GetFamilyName() << std::endl;

	//LogFile << "Dumping files..." << std::endl;
	//LogFile << "Dump path:\n\t" << DumpPath << std::endl;

	IPC::PushMessage(L"UWPDumper Build date(%s : %s)\n", __DATE__, __TIME__);
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
	#define Wrap 50
		const fs::path WritePath = DumpPath + File.path().wstring().substr(1);
		const std::wstring ReadPath = File.path().wstring();
		IPC::PushMessage(
			L"%*.*s %*.1f%%\n",
			Wrap, Wrap,
			ReadPath.c_str() + (ReadPath.length() > Wrap ? (ReadPath.length() - Wrap) : 0),
			Wrap,
			(i / static_cast<float>(FileList.size())) * 100
		);

		fs::create_directories(WritePath.parent_path());
		fs::copy(
			File.path(),
			WritePath,
			fs::copy_options::update_existing
		);
		i++;
	}

	IPC::PushMessage(L"Dump complete Path:\n\t%s\n", DumpPath.c_str());

	//try
	//{
	//	fs::copy(
	//		UWP::Current::GetPackagePath(),
	//		DumpPath,
	//		fs::copy_options::recursive | fs::copy_options::update_existing
	//	);
	//}
	//catch( fs::filesystem_error &e )
	//{
	//	LogFile << "Error dumping: " << e.what() << std::endl;
	//	LogFile << "\tOperand 1: " << e.path1() << std::endl;
	//	LogFile << "\tOperand 2: " << e.path2() << std::endl;
	//}
	//LogFile << "Dumping complete!" << std::endl;

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