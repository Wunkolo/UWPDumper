#include <iostream>
#include <iomanip>
#include <string>
#include <memory>
#include <chrono>

#include <windows.h>
#include <psapi.h> //GetModuleFileNameEx
#include <TlHelp32.h>

// Setting DLL access controls
#include <AccCtrl.h>
#include <Aclapi.h>
#include <Sddl.h>

// UWP
#include <atlbase.h>
#include <appmodel.h>

// IPC
#include <UWP/DumperIPC.hpp>

const wchar_t* DLLFile = L"UWPDumper.dll";

void SetAccessControl(
	const std::wstring& ExecutableName,
	const wchar_t* AccessString
);

// Returns handle to RemoteThread on success
void* DLLInjectRemote(uint32_t ProcessID, const std::wstring& DLLpath);

std::wstring GetRunningDirectory();

std::wstring GetLastErrorMessage();

using ThreadCallback = bool(*)(
	std::uint32_t ThreadID,
	void* Data
);

void IterateThreads(ThreadCallback ThreadProc, std::uint32_t ProcessID, void* Data)
{
	void* hSnapShot = CreateToolhelp32Snapshot(
		TH32CS_SNAPTHREAD,
		ProcessID
	);

	if( hSnapShot == INVALID_HANDLE_VALUE )
	{
		return;
	}

	THREADENTRY32 ThreadEntry = { 0 };
	ThreadEntry.dwSize = sizeof(THREADENTRY32);
	Thread32First(hSnapShot, &ThreadEntry);
	do
	{
		if( ThreadEntry.th32OwnerProcessID == ProcessID )
		{
			const bool Continue = ThreadProc(
				ThreadEntry.th32ThreadID,
				Data
			);
			if( Continue == false )
			{
				break;
			}
		}
	}
	while( Thread32Next(hSnapShot, &ThreadEntry) );

	CloseHandle(hSnapShot);
}

int main()
{
	// Enable VT100
	DWORD ConsoleMode;
	GetConsoleMode(
		GetStdHandle(STD_OUTPUT_HANDLE),
		&ConsoleMode
	);
	SetConsoleMode(
		GetStdHandle(STD_OUTPUT_HANDLE),
		ConsoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING
	);
	SetConsoleOutputCP(437);

	std::wcout << "\033[92mUWPInjector Build date (" << __DATE__ << " : " << __TIME__ << ')' << std::endl;
	std::wcout << "\033[96m\t\033(0m\033(Bhttps://github.com/Wunkolo/UWPDumper\n";
	std::wcout << "\033[95m\033(0" << std::wstring(80, 'q') << "\033(B" << std::endl;

	std::uint32_t ProcessID = 0;

	IPC::SetClientProcess(GetCurrentProcessId());

	std::cout << "\033[93mCurrently running UWP Apps:" << std::endl;
	void* ProcessSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 ProcessEntry;
	ProcessEntry.dwSize = sizeof(PROCESSENTRY32);

	if( Process32First(ProcessSnapshot, &ProcessEntry) )
	{
		while( Process32Next(ProcessSnapshot, &ProcessEntry) )
		{
			void* ProcessHandle = OpenProcess(
				PROCESS_QUERY_LIMITED_INFORMATION,
				false,
				ProcessEntry.th32ProcessID
			);
			if( ProcessHandle )
			{
				std::uint32_t NameLength = 0;
				std::int32_t ProcessCode = GetPackageFamilyName(
					ProcessHandle,
					&NameLength,
					nullptr
				);
				if( NameLength )
				{
					std::wcout
						<< "\033[92m"
						<< std::setw(12)
						<< ProcessEntry.th32ProcessID;

					std::wcout
						<< "\033[96m"
						<< " \033(0x\033(B "
						<< ProcessEntry.szExeFile << " :\n\t\t\033(0m\033(B";
					std::unique_ptr<wchar_t[]> PackageName(new wchar_t[NameLength]());

					ProcessCode = GetPackageFamilyName(
						ProcessHandle,
						&NameLength,
						PackageName.get()
					);

					if( ProcessCode != ERROR_SUCCESS )
					{
						std::wcout << "GetPackageFamilyName Error: " << ProcessCode;
					}

					std::wcout << PackageName.get() << std::endl;

					PackageName.reset();
				}
			}
			CloseHandle(ProcessHandle);
		}
	}
	else
	{
		std::cout << "\033[91mUnable to iterate active processes" << std::endl;
		system("pause");
		return EXIT_FAILURE;
	}
	std::cout << "\033[93mEnter ProcessID: \033[92m";
	std::cin >> ProcessID;

	SetAccessControl(GetRunningDirectory() + L'\\' + DLLFile, L"S-1-15-2-1");

	IPC::SetTargetProcess(ProcessID);

	std::cout << "\033[93mInjecting into remote process: ";
	void* RemoteThread = DLLInjectRemote(ProcessID, GetRunningDirectory() + L'\\' + DLLFile);
	if( RemoteThread == nullptr)
	{
		std::cout << "\033[91mFailed" << std::endl;
		system("pause");
		return EXIT_FAILURE;
	}
	std::uint32_t RemoteThreadID = GetThreadId(RemoteThread);
	std::cout << "\033[92mSuccess! (" << std::hex << RemoteThreadID << ')' << std::endl;

	// Wait for remote thread to signal back
	std::cout << "\033[93mWaiting for remote thread to report back..." << std::endl;
	std::chrono::high_resolution_clock::time_point ThreadTimeout = std::chrono::high_resolution_clock::now() + std::chrono::seconds(5);
	while( IPC::GetTargetThread() != RemoteThreadID )
	{
		if( WaitForSingleObject(RemoteThread,0) == WAIT_OBJECT_0 )
		{
			std::cout
				<<
					"\033[91mRemote thread has terminated before it could report back\n"
					"It is likely a remote error has occured"
				<< std::endl;
			system("pause");
			return EXIT_FAILURE;
		}
		if( std::chrono::high_resolution_clock::now() >= ThreadTimeout )
		{
			std::cout << "\033[91mRemote thread wait timeout: Remote thread did not report back" << std::endl;
			std::uint32_t ThreadExitCode = 0;
			
			if( !GetExitCodeThread(RemoteThread, reinterpret_cast<LPDWORD>(&ThreadExitCode)) )
			{
				std::cout << "Failed to get remote thread Exit Code" << std::endl;
				std::wcout << GetLastErrorMessage() << std::endl;
			}
			std::cout << "\033[91mRemote thread exit code: " << std::hex << ThreadExitCode << std::endl;
			if( ThreadExitCode == 0 )
			{
				std::wcout << GetLastErrorMessage() << std::endl;
			}
			system("pause");
			return EXIT_FAILURE;
		}
		std::printf(
			"%c\r",
			"-\\|/"[
				(std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()
				).count() / 500) % 4
			]
		);
	}

	std::cout << "Remote Dumper thread found: 0x" << std::hex << IPC::GetTargetThread() << std::endl;

	std::cout << "\033[0m" << std::flush;
	while( IPC::GetTargetThread() != IPC::InvalidThread )
	{
		while( IPC::MessageCount() > 0 )
		{
			const std::wstring CurMessage = IPC::PopMessage();
			std::wcout << CurMessage << "\033[0m";
		}
	}
	system("pause");
	return EXIT_SUCCESS;
}

void SetAccessControl(const std::wstring& ExecutableName, const wchar_t* AccessString)
{
	PSECURITY_DESCRIPTOR SecurityDescriptor = nullptr;
	EXPLICIT_ACCESSW ExplicitAccess = { 0 };

	ACL* AccessControlCurrent = nullptr;
	ACL* AccessControlNew = nullptr;

	SECURITY_INFORMATION SecurityInfo = DACL_SECURITY_INFORMATION;
	PSID SecurityIdentifier = nullptr;

	if(
		GetNamedSecurityInfoW(
			ExecutableName.c_str(),
			SE_FILE_OBJECT,
			DACL_SECURITY_INFORMATION,
			nullptr,
			nullptr,
			&AccessControlCurrent,
			nullptr,
			&SecurityDescriptor
		) == ERROR_SUCCESS
	)
	{
		ConvertStringSidToSidW(AccessString, &SecurityIdentifier);
		if( SecurityIdentifier != nullptr )
		{
			ExplicitAccess.grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE;
			ExplicitAccess.grfAccessMode = SET_ACCESS;
			ExplicitAccess.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
			ExplicitAccess.Trustee.TrusteeForm = TRUSTEE_IS_SID;
			ExplicitAccess.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
			ExplicitAccess.Trustee.ptstrName = reinterpret_cast<wchar_t*>(SecurityIdentifier);

			if(
				SetEntriesInAclW(
					1,
					&ExplicitAccess,
					AccessControlCurrent,
					&AccessControlNew
				) == ERROR_SUCCESS
			)
			{
				SetNamedSecurityInfoW(
					const_cast<wchar_t*>(ExecutableName.c_str()),
					SE_FILE_OBJECT,
					SecurityInfo,
					nullptr,
					nullptr,
					AccessControlNew,
					nullptr
				);
			}
		}
	}
	if( SecurityDescriptor )
	{
		LocalFree(reinterpret_cast<HLOCAL>(SecurityDescriptor));
	}
	if( AccessControlNew )
	{
		LocalFree(reinterpret_cast<HLOCAL>(AccessControlNew));
	}
}

void* DLLInjectRemote(std::uint32_t ProcessID, const std::wstring& DLLpath)
{
	const std::size_t DLLPathSize = ((DLLpath.size() + 1) * sizeof(wchar_t));
	std::uint32_t Result;
	if( !ProcessID )
	{
		std::wcout << "Invalid Process ID: " << ProcessID << std::endl;
		return nullptr;
	}

	if( GetFileAttributesW(DLLpath.c_str()) == INVALID_FILE_ATTRIBUTES )
	{
		std::wcout << "DLL file: " << DLLpath << " does not exists" << std::endl;
		return nullptr;
	}

	SetAccessControl(DLLpath, L"S-1-15-2-1");

	void* ProcLoadLibrary = reinterpret_cast<void*>(
		GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW")
	);

	if( !ProcLoadLibrary )
	{
		std::wcout << "Unable to find LoadLibraryW procedure" << std::endl;
		return nullptr;
	}

	void* Process = OpenProcess(PROCESS_ALL_ACCESS, false, ProcessID);
	if( Process == nullptr )
	{
		std::wcout << "Unable to open process ID" << ProcessID << " for writing" << std::endl;
		return nullptr;
	}
	// Allocate remote memory containing the path to the dll
	void* RemoteDLLPath = reinterpret_cast<void*>(
		VirtualAllocEx(
			Process,
			nullptr,
			DLLPathSize,
			MEM_RESERVE | MEM_COMMIT,
			PAGE_READWRITE
		)
	);

	if( RemoteDLLPath == nullptr )
	{
		std::wcout << "Unable to remotely allocate memory" << std::endl;
		CloseHandle(Process);
		return nullptr;
	}

	std::size_t BytesWritten = 0;
	Result = WriteProcessMemory(
		Process,
		RemoteDLLPath,
		DLLpath.data(),
		DLLPathSize,
		&BytesWritten
	);

	if( Result == 0 )
	{
		std::wcout << "Unable to write process memory" << std::endl;
		CloseHandle(Process);
		return nullptr;
	}

	if( BytesWritten != DLLPathSize )
	{
		std::wcout << "Failed to write remote DLL path name" << std::endl;
		CloseHandle(Process);
		return nullptr;
	}

	void* RemoteThread =
		CreateRemoteThread(
			Process,
			nullptr,
			0,
			reinterpret_cast<LPTHREAD_START_ROUTINE>(ProcLoadLibrary),
			RemoteDLLPath,
			0,
			nullptr
		);
	if( RemoteThread == nullptr )
	{
		// Failed to create thread
		std::wcout << "Unable to create remote thread" << std::endl;
		CloseHandle(Process);
		return nullptr;
	}

	if( WaitForSingleObject(RemoteThread, 0) == WAIT_OBJECT_0 )
	{
		std::wcout << "Remote thread exited soon after creation" << std::endl;
		return nullptr;
	}

	// Set thread priority to highest
	if( SetThreadPriority(RemoteThread, THREAD_PRIORITY_HIGHEST) == 0 )
	{
		// Failed to set thread priority, whatever
	}


	// Don't wait for remote thread to finish anymore, the dump can happen within DllMain

	// const std::uint32_t WaitResult = WaitForSingleObject(RemoteThread, INFINITE);
	// std::wcout << "Remote thread completed with code: " << std::hex << WaitResult << std::endl;
	// if( WaitResult == WAIT_FAILED )
	// {
	// 	std::wcout << GetLastErrorMessage() << std::endl;
	// }
	// CloseHandle(RemoteThread);

	// Let RemoteDLLPath leak
	// VirtualFreeEx(Process, RemoteDLLPath, 0, MEM_RELEASE);

	// Dont need the process handle anymore
	CloseHandle(Process);
	return RemoteThread;
}

std::wstring GetRunningDirectory()
{
	wchar_t RunPath[MAX_PATH];
	GetModuleFileNameW(GetModuleHandleW(nullptr), RunPath, MAX_PATH);
	PathRemoveFileSpecW(RunPath);
	return std::wstring(RunPath);
}

std::wstring GetLastErrorMessage()
{
	wchar_t Buffer[256] = {0};
	FormatMessageW(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		Buffer,
		(sizeof(Buffer) / sizeof(wchar_t)),
		nullptr
	);
	return std::wstring(Buffer, 256);
}