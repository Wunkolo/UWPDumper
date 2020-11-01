#include <iostream>
#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <memory>
#include <chrono>
#include <filesystem>

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

#define REPARSE_MOUNTPOINT_HEADER_SIZE   8

const wchar_t* DLLFile = L"UWPDumper.dll";

void SetAccessControl(
	const std::wstring& ExecutableName,
	const wchar_t* AccessString
);

bool DLLInjectRemote(uint32_t ProcessID, const std::wstring& DLLpath);

std::wstring GetRunningDirectory();

using ThreadCallback = bool(*)(
	std::uint32_t ThreadID,
	void* Data
);

typedef struct {
	DWORD ReparseTag;
	DWORD ReparseDataLength;
	WORD Reserved;
	WORD ReparseTargetLength;
	WORD ReparseTargetMaximumLength;
	WORD Reserved1;
	WCHAR ReparseTarget[1];
} REPARSE_MOUNTPOINT_DATA_BUFFER, * PREPARSE_MOUNTPOINT_DATA_BUFFER;

static DWORD CreateJunction(LPCSTR szJunction, LPCSTR szPath)
{
	DWORD LastError = ERROR_SUCCESS;
	std::byte buf[sizeof(REPARSE_MOUNTPOINT_DATA_BUFFER) + MAX_PATH * sizeof(WCHAR)] = {};
	REPARSE_MOUNTPOINT_DATA_BUFFER& ReparseBuffer = (REPARSE_MOUNTPOINT_DATA_BUFFER&)buf;
	char szTarget[MAX_PATH] = "\\??\\";

	strcat_s(szTarget, szPath);
	// strcat_s(szTarget, "\\");

	if( !CreateDirectory(szJunction, nullptr) ) return GetLastError();

	// Obtain SE_RESTORE_NAME privilege (required for opening a directory)
	HANDLE hToken = nullptr;
	TOKEN_PRIVILEGES tp;
	try {
		if( !OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) throw GetLastError();
		if( !LookupPrivilegeValue(nullptr, SE_RESTORE_NAME, &tp.Privileges[0].Luid))  throw GetLastError();
		tp.PrivilegeCount = 1;
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		if( !AdjustTokenPrivileges(hToken, false, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr) )  throw GetLastError();
	}
	catch (DWORD LastError)
	{
		if( hToken ) CloseHandle(hToken);
		return LastError;
	}
	if( hToken ) CloseHandle(hToken);

	const HANDLE hDir = CreateFile(szJunction, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	if( hDir == INVALID_HANDLE_VALUE ) return GetLastError();

	ReparseBuffer.ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
	int32_t len = MultiByteToWideChar(CP_ACP, 0, szTarget, -1, ReparseBuffer.ReparseTarget, MAX_PATH);
	ReparseBuffer.ReparseTargetMaximumLength = static_cast<WORD>((len--) * sizeof(WCHAR));
	ReparseBuffer.ReparseTargetLength = static_cast<WORD>(len * sizeof(WCHAR));
	ReparseBuffer.ReparseDataLength = ReparseBuffer.ReparseTargetLength + 12;

	DWORD dwRet;
	if( !DeviceIoControl(hDir, FSCTL_SET_REPARSE_POINT, &ReparseBuffer, ReparseBuffer.ReparseDataLength + REPARSE_MOUNTPOINT_HEADER_SIZE, nullptr, 0, &dwRet, nullptr) )
	{
		LastError = GetLastError();
		CloseHandle(hDir);
		RemoveDirectory(szJunction);
		return LastError;
	}

	CloseHandle(hDir);
	return ERROR_SUCCESS;
}


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



int main(int argc, char** argv, char** envp)
{
	std::uint32_t ProcessID = 0;
	std::filesystem::path TargetPath("C:\\");
	bool Logging = false;
	if( argc > 1 )
	{
		for( std::size_t i = 1; i < argc; ++i )
		{
			if( std::string_view(argv[i]) == "-h" )
			{
				std::cout << "To Set PID:\n";
				std::cout << "	  -p {pid}\n";
				std::cout << "EG: -p 1234\n";
				std::cout << "To Enable Logging:\n";
				std::cout << "    -l \n";
				std::cout << "To Dump To A Custom Folder:\n";
				std::cout << "    -d {folder} \n";
				std::cout << "EG: -d D:\\uwp\\dumps\\calculator \n";
				system("pause");
				return 0;
			}
			else if( std::string_view(argv[i]) == "-p" )
			{
				if( i != argc)
				{
					ProcessID = (std::uint32_t)atoi(argv[i + 1]);
				}
				else
				{
					std::cout << "-p must be followed by a pid\n";
					system("pause");
					return 0;
				}
			}
			else if( std::string_view(argv[i]) == "-l" )
			{
				Logging = true;
			}
			else if( std::string_view(argv[i]) == "-d" )
			{
				if( i != argc )
				{
					TargetPath = argv[i + 1];
				}
				else
				{
					std::cout << "-d must be followed by the custom location\n";
					system("pause");
					return 0;
				}
			}
		}
	}
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

	IPC::SetClientProcess(GetCurrentProcessId());

	if( ProcessID == 0)
	{
		std::cout << "\033[93mCurrently running UWP Apps:" << std::endl;
		void* ProcessSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		PROCESSENTRY32 ProcessEntry;
		ProcessEntry.dwSize = sizeof(PROCESSENTRY32);

		if( Process32First(ProcessSnapshot, &ProcessEntry) )
		{
			while (Process32Next(ProcessSnapshot, &ProcessEntry) )
			{
				void* ProcessHandle = OpenProcess(
					PROCESS_QUERY_LIMITED_INFORMATION,
					false,
					ProcessEntry.th32ProcessID
				);
				if( ProcessHandle)
				{
					std::uint32_t NameLength = 0;
					std::int32_t ProcessCode = GetPackageFamilyName(
						ProcessHandle, &NameLength, nullptr
					);
					if( NameLength)
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

						if( ProcessCode != ERROR_SUCCESS)
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
	}

	// Get package name
	std::wstring PackageFileName;

	if( 
		HANDLE ProcessHandle = OpenProcess(
			PROCESS_ALL_ACCESS | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
			false, ProcessID
		); ProcessHandle
		)
	{
		std::uint32_t NameLength = 0;
		std::int32_t ProcessCode = GetPackageFamilyName(
			ProcessHandle, &NameLength, nullptr
		);
		if( NameLength )
		{
			std::unique_ptr<wchar_t[]> PackageName(new wchar_t[NameLength]());

			ProcessCode = GetPackageFamilyName(
				ProcessHandle, &NameLength, PackageName.get()
			);

			if( ProcessCode != ERROR_SUCCESS )
			{
				std::wcout << "GetPackageFamilyName Error: " << ProcessCode;
			}
			PackageFileName = PackageName.get();
		}
		CloseHandle(ProcessHandle);
	}
	else
	{
		std::cout << "\033[91mFailed to query process for " << std::endl;
		system("pause");
		return EXIT_FAILURE;
	}

	//get local app data folder
	std::wofstream LogFile;
	char* LocalAppData;
	size_t len;
	errno_t err = _dupenv_s(&LocalAppData, &len, "LOCALAPPDATA");

	if( TargetPath != std::filesystem::path("C:\\") )
	{
		//get dump folder path
		std::filesystem::path DumpFolderPath(LocalAppData);
		DumpFolderPath.append("Packages");
		DumpFolderPath.append(PackageFileName);
		DumpFolderPath.append("TempState\\DUMP");
		//clear out dump folder
		std::filesystem::remove_all(DumpFolderPath);
		//create junction
		CreateJunction(DumpFolderPath.string().c_str(), TargetPath.string().c_str());
		//set acl for target directory
		SetAccessControl(TargetPath, L"S-1-15-2-1");
	}

	SetAccessControl(GetRunningDirectory() + L'\\' + DLLFile, L"S-1-15-2-1");

	IPC::SetTargetProcess(ProcessID);

	std::cout << "\033[93mInjecting into remote process: ";
	if( !DLLInjectRemote(ProcessID, GetRunningDirectory() + L'\\' + DLLFile) )
	{
		std::cout << "\033[91mFailed" << std::endl;
		system("pause");
		return EXIT_FAILURE;
	}
	std::cout << "\033[92mSuccess!" << std::endl;

	std::cout << "\033[93mWaiting for remote thread IPC:" << std::endl;
	std::chrono::high_resolution_clock::time_point ThreadTimeout = std::chrono::high_resolution_clock::now() + std::chrono::seconds(5);
	while( IPC::GetTargetThread() == IPC::InvalidThread )
	{
		if( std::chrono::high_resolution_clock::now() >= ThreadTimeout )
		{
			std::cout << "\033[91mRemote thread wait timeout: Unable to find target thread" << std::endl;
			system("pause");
			return EXIT_FAILURE;
		}
	}

	std::cout << "Remote Dumper thread found: 0x" << std::hex << IPC::GetTargetThread() << std::endl;
	
	if( Logging )
	{
		std::filesystem::path LogFilePath = std::filesystem::current_path();
		//add package Name to logfile Path
		LogFilePath.append(PackageFileName);
		LogFilePath.concat(" ");

		// Get timestamp for log file name
		{
			std::time_t CurrentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			std::stringstream TimeString;
			struct tm TimeBuffer;
			gmtime_s(&TimeBuffer, &CurrentTime);
			TimeString << std::put_time(&TimeBuffer, "%Y%m%dT%H%M%S");
			LogFilePath.concat(TimeString.str());
		}

		// Attempt to create file
		LogFilePath.concat(".txt");
		std::cout << LogFilePath << std::endl;
		LogFile = std::wofstream(LogFilePath);
		if( LogFile.is_open() )
		{
			std::cout << "\033[92mLogging to File: " << LogFilePath << "\033[0m" << std::endl;
		}
		else
		{
			std::cout << "\033[91mFailed to open log file\033[0m" << std::endl;;
			system("pause");
			return EXIT_FAILURE;
		}
	}

	std::cout << "\033[0m" << std::flush;
	std::wstring CurMessage;
	CurMessage.reserve(IPC::MessageEntry::StringSize);
	while( IPC::GetTargetThread() != IPC::InvalidThread )
	{
		while( IPC::PopMessage(CurMessage) )
		{
			std::wcout << CurMessage << "\033[0m";
			if( Logging )
			{
				LogFile << CurMessage << std::flush;
			}
		}
	}
	if( Logging ) LogFile.close();
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
			ExplicitAccess.grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE | GENERIC_WRITE;
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
		GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW")
	);

	if( !ProcLoadLibrary )
	{
		std::wcout << "Unable to find LoadLibraryW procedure" << std::endl;
		return false;
	}

	void* Process = OpenProcess(PROCESS_ALL_ACCESS, false, ProcessID);
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