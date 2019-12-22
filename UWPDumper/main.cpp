#include <cstddef>
#include <cstddef>
#include <fstream>
#include <iomanip>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <ShlObj.h>

#include <windows.storage.h>
#include <windows.system.h>
#include <wrl.h>

#define WIDEIFYIMP(x) L##x
#define WIDEIFY(x) WIDEIFYIMP(x)

#include <queue>

#include <filesystem>
namespace fs = std::filesystem;

#include <UWP/UWP.hpp>

#include <UWP/DumperIPC.hpp>

void OpenTempState()
{
	Microsoft::WRL::ComPtr<ABI::Windows::Storage::IApplicationDataStatics> AppDataStatics;

	if(
		RoGetActivationFactory(
			Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Storage_ApplicationData).Get(),
			__uuidof(AppDataStatics), &AppDataStatics
		) < 0
	)
	{
		// Error getting ApplicationData statics
	}

	Microsoft::WRL::ComPtr<ABI::Windows::Storage::IApplicationData> AppData;
	if( AppDataStatics->get_Current(&AppData) < 0 )
	{
		// Error getting current IApplicationData
	}

	Microsoft::WRL::ComPtr<ABI::Windows::System::ILauncherStatics3> LauncherStatics;
	if(
		RoGetActivationFactory(
			Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_System_Launcher).Get(),
			__uuidof(LauncherStatics), &LauncherStatics
		) < 0
	)
	{
		// Error getting Launcher statics
	}

	Microsoft::WRL::ComPtr<ABI::Windows::Storage::IStorageFolder> TemporaryFolder;

	if( AppData->get_TemporaryFolder(&TemporaryFolder) < 0 )
	{
		// Failed to get folder
		return;
	}

	Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IAsyncOperation<bool>> Result;
	LauncherStatics->LaunchFolderAsync(
		TemporaryFolder.Get(),
		&Result
	);
}

std::uint32_t __stdcall DumperThread(void* DLLHandle)
{
	std::wstring DumpPath = fs::path(UWP::Current::Storage::GetTemporaryPath()) / L"DUMP";

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

	for( auto& Entry : fs::recursive_directory_iterator(UWP::Current::GetPackagePath()) )
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
		try
		{

			const fs::path WritePath = DumpPath + File.path().wstring().substr(UWP::Current::GetPackagePath().length());
			const std::wstring ReadPath = File.path().wstring();
			IPC::PushMessage(
				L"%*.*s %*.u bytes %*zu/%zu\n",
				60, 60,
				ReadPath.c_str() + (ReadPath.length() > 60 ? (ReadPath.length() - (60)) : 0),
				15,
				fs::file_size(File),
				20,
				++i,
				FileList.size()
			);

			std::error_code ErrorCode;
			if( fs::create_directories(WritePath.parent_path(), ErrorCode) == false && ErrorCode )
			{
				const std::string ErrorMessage(ErrorCode.message());
				std::wstring WErrorMessage;
				WErrorMessage.assign(ErrorMessage.begin(), ErrorMessage.end());
				IPC::PushMessage(
					L"Error creating subfolder: %s\n\t%s\n",
					WritePath.parent_path().c_str(),
					WErrorMessage.c_str()
				);
				continue;
			}

			std::ifstream SourceFile(ReadPath, std::ios::binary);
			if( !SourceFile.is_open() )
			{
				IPC::PushMessage(
					L"Error opening %s for reading\n",
					ReadPath.c_str()
				);
				continue;
			}

			std::ofstream DestFile(WritePath, std::ios::binary);
			if( !DestFile.is_open() )
			{
				IPC::PushMessage(
					L"Error opening %s for writing\n",
					WritePath.c_str()
				);
				continue;
			}

			if( SourceFile && DestFile )
			{
				DestFile << SourceFile.rdbuf();
			}
			else
			{
				IPC::PushMessage(
					L"Error copying file data:\n"
					"\t%s\n"
					"\tto\n"
					"\t%s\n",
					File.path().c_str(),
					WritePath.c_str()
				);
			}
		}
		catch( std::exception& Exception )
		{
			//std::wstring ExceptionMessage = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>{}.from_bytes( Exception.what() );

			IPC::PushMessage(
				L"Exception {%s}:\n"
				"\t[%s]\n",
				File.path().c_str(),
				Exception.what()
			);
		}
	}

	IPC::PushMessage(L"Dump complete!\n\tPath:\n\t%s\n", DumpPath.c_str());
	OpenTempState();
	IPC::ClearTargetThread();

	FreeLibraryAndExitThread(
		reinterpret_cast<HMODULE>(DLLHandle),
		EXIT_SUCCESS
	);
}

std::int32_t __stdcall DllMain(HINSTANCE hDLL, std::uint32_t Reason, void* Reserved)
{
	switch( Reason )
	{
	case DLL_PROCESS_ATTACH:
	{
		IPC::PushMessage(L"DLL Attached to process %u\n", GetCurrentProcessId());
		if( IPC::GetTargetProcess() == GetCurrentProcessId() )
		{
			IPC::PushMessage(L"Creating dumper thread%u\n");
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
