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

// Console
#include <MinConsole.hpp>

namespace Console = MinConsole;

const wchar_t* DLLFile = L"UWPDumper.dll";

void SetAccessControl(
	const std::wstring& ExecutableName,
	const wchar_t* AccessString
);

bool DLLInjectRemote(uint32_t ProcessID, const std::wstring& DLLpath);

std::wstring GetRunningDirectory();

int main()
{
	SetConsoleOutputCP(437);

	Console::SetTextColor(Console::Color::Green | Console::Color::Bright);
	std::wcout << "UWPInjector Build date (" << __DATE__ << " : " << __TIME__ << ')' << std::endl;
	Console::SetTextColor(Console::Color::Input);
	std::wcout << "\t-https://github.com/Wunkolo/UWPDumper\n";
	Console::SetTextColor(Console::Color::Magenta);
	std::wcout << std::wstring(Console::GetWidth() - 1, '-') << std::endl;
	Console::SetTextColor(Console::Color::Info);

	std::uint32_t ProcessID = 0;

	IPC::SetClientProcess(GetCurrentProcessId());

	std::cout << "Currently running UWP Apps:" << std::endl;
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
					Console::SetTextColor(Console::Color::Green | Console::Color::Bright);
					std::wcout
						<< std::setw(12)
						<< ProcessEntry.th32ProcessID;

					Console::SetTextColor(Console::Color::Info);
					std::wcout
						<< " | "
						<< ProcessEntry.szExeFile << " :\n\t\t-";
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
		Console::SetTextColor(Console::Color::Red | Console::Color::Bright);
		std::cout << "Unable to iterate active processes" << std::endl;
		system("pause");
		return EXIT_FAILURE;
	}
	std::cout << "Enter ProcessID: ";
	Console::SetTextColor(Console::Color::Green | Console::Color::Bright);
	std::cin >> ProcessID;
	Console::SetTextColor(Console::Color::Info);

	SetAccessControl(GetRunningDirectory() + L'\\' + DLLFile, L"S-1-15-2-1");

	IPC::SetTargetProcess(ProcessID);

	std::cout << "Injecting into remote process: ";
	if( !DLLInjectRemote(ProcessID, GetRunningDirectory() + L'\\' + DLLFile) )
	{
		Console::SetTextColor(Console::Color::Red | Console::Color::Bright);
		std::cout << "Failed" << std::endl;
		system("pause");
	}
	Console::SetTextColor(Console::Color::Green | Console::Color::Bright);
	std::cout << "Success!" << std::endl;

	Console::SetTextColor(Console::Color::Info);
	std::cout << "Waiting for remote thread IPC:" << std::endl;
	std::chrono::high_resolution_clock::time_point ThreadTimeout = std::chrono::high_resolution_clock::now() + std::chrono::seconds(5);
	while( IPC::GetTargetThread() == IPC::InvalidThread )
	{
		if( std::chrono::high_resolution_clock::now() >= ThreadTimeout )
		{
			Console::SetTextColor(Console::Color::Red | Console::Color::Bright);
			std::cout << "Remote thread wait timeout: Unable to find target thread" << std::endl;
			return EXIT_FAILURE;
		}
	}

	std::cout << "Remote Dumper thread found: 0x" << std::hex << IPC::GetTargetThread() << std::endl;

	Console::SetTextColor(
		Console::Color::Cyan | Console::Color::Bright
	);
	while( IPC::GetTargetThread() != IPC::InvalidThread )
	{
		while( IPC::MessageCount() > 0 )
		{
			std::wcout << IPC::PopMessage();
		}
	}
	system("pause");
	return EXIT_SUCCESS;
}

void SetAccessControl(const std::wstring& ExecutableName, const wchar_t* AccessString)
{
	PSECURITY_DESCRIPTOR SecurityDescriptor = nullptr;
	EXPLICIT_ACCESSW ExplicitAccess = {0};

	ACL* AccessControlCurrent = nullptr;
	ACL* AccessControlNew = nullptr;

	SECURITY_INFORMATION SecurityInfo = DACL_SECURITY_INFORMATION;
	PSID SecurityIdentifier = nullptr;

	if( GetNamedSecurityInfoW(
			ExecutableName.c_str(),
			SE_FILE_OBJECT,
			DACL_SECURITY_INFORMATION,
			nullptr,
			nullptr,
			&AccessControlCurrent,
			nullptr,
			&SecurityDescriptor) == ERROR_SUCCESS
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

			if( SetEntriesInAclW(
				1,
				&ExplicitAccess,
				AccessControlCurrent,
				&AccessControlNew) == ERROR_SUCCESS )
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
		LocalFree(
			reinterpret_cast<HLOCAL>(SecurityDescriptor)
		);
	}
	if( AccessControlNew )
	{
		LocalFree(
			reinterpret_cast<HLOCAL>(AccessControlNew)
		);
	}
}

bool DLLInjectRemote(uint32_t ProcessID, const std::wstring& DLLpath)
{
	const std::size_t DLLPathSize = ((DLLpath.size() + 1) * sizeof(wchar_t));
	std::uint32_t Result;
	if( !ProcessID )
	{
		std::wcout << "Invalid Process ID: " << ProcessID << std::endl;
		return false;
	}

	if( GetFileAttributesW(DLLpath.c_str()) == INVALID_FILE_ATTRIBUTES )
	{
		std::wcout << "DLL file: " << DLLpath << " does not exists" << std::endl;
		return false;
	}

	SetAccessControl(DLLpath, L"S-1-15-2-1");

	void* ProcLoadLibrary = reinterpret_cast<void*>(
		GetProcAddress(
			GetModuleHandleW(L"kernel32.dll"),
			"LoadLibraryW")
	);

	if( !ProcLoadLibrary )
	{
		std::wcout << "Unable to find LoadLibraryW procedure" << std::endl;
		return false;
	}

	void* Process = OpenProcess(
		PROCESS_ALL_ACCESS,
		false,
		ProcessID);
	if( Process == nullptr )
	{
		std::wcout << "Unable to open process ID" << ProcessID << " for writing" << std::endl;
		return false;
	}
	void* VirtualAlloc = reinterpret_cast<void*>(
		VirtualAllocEx(
			Process,
			nullptr,
			DLLPathSize,
			MEM_RESERVE | MEM_COMMIT,
			PAGE_READWRITE
		)
	);

	if( VirtualAlloc == nullptr )
	{
		std::wcout << "Unable to remotely allocate memory" << std::endl;
		CloseHandle(Process);
		return false;
	}

	SIZE_T BytesWritten = 0;
	Result = WriteProcessMemory(
		Process,
		VirtualAlloc,
		DLLpath.data(),
		DLLPathSize,
		&BytesWritten
	);

	if( Result == 0 )
	{
		std::wcout << "Unable to write process memory" << std::endl;
		CloseHandle(Process);
		return false;
	}

	if( BytesWritten != DLLPathSize )
	{
		std::wcout << "Failed to write remote DLL path name" << std::endl;
		CloseHandle(Process);
		return false;
	}

	void* RemoteThread =
		CreateRemoteThread(
			Process,
			nullptr,
			0,
			reinterpret_cast<LPTHREAD_START_ROUTINE>(ProcLoadLibrary),
			VirtualAlloc,
			0,
			nullptr
		);

	// Wait for remote thread to finish
	if( RemoteThread )
	{
		// Explicitly wait for LoadLibraryW to complete before releasing memory
		// avoids causing a remote memory leak
		WaitForSingleObject(RemoteThread, INFINITE);
		CloseHandle(RemoteThread);
	}
	else
	{
		// Failed to create thread
		std::wcout << "Unable to create remote thread" << std::endl;
	}

	VirtualFreeEx(Process, VirtualAlloc, 0, MEM_RELEASE);
	CloseHandle(Process);
	return true;
}

std::wstring GetRunningDirectory()
{
	wchar_t RunPath[MAX_PATH];
	GetModuleFileNameW(GetModuleHandleW(nullptr), RunPath, MAX_PATH);
	PathRemoveFileSpecW(RunPath);
	return std::wstring(RunPath);
}
