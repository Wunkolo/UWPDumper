#include "UWP.hpp"

#include <Windows.h>
#include <ShlObj.h>
#include <appmodel.h>
#include <AppxPackaging.h>

#include <memory>

namespace
{
std::shared_ptr<PACKAGE_ID> GetPackageIdentifier()
{
	uint32_t Size = 0;
	GetCurrentPackageId(&Size, nullptr);

	std::shared_ptr<PACKAGE_ID> Result = std::make_shared<PACKAGE_ID>();

	if( GetCurrentPackageId(
		&Size,
		reinterpret_cast<uint8_t*>(Result.get())
	) != ERROR_SUCCESS )
	{
		return nullptr;
	}
	return Result;
}

std::shared_ptr<PACKAGE_INFO> GetPackageInfo()
{
	uint32_t Size = 0;
	uint32_t Count = 0;
	GetCurrentPackageInfo(PACKAGE_FILTER_HEAD, &Size, nullptr, &Count);

	std::shared_ptr<PACKAGE_INFO> Result(
		reinterpret_cast<PACKAGE_INFO*>(malloc(Size)),
		[](PACKAGE_INFO *PackageInfo)
	{
		free(PackageInfo);
	}
	);

	if( GetCurrentPackageInfo(
		PACKAGE_FILTER_HEAD,
		&Size,
		reinterpret_cast<uint8_t*>(Result.get()),
		&Count) != ERROR_SUCCESS )
	{
		return nullptr;
	}
	return Result;
}
}

std::wstring UWP::Current::GetFamilyName()
{
	std::wstring FamilyName;
	uint32_t FamilyNameSize = 0;

	GetCurrentPackageFamilyName(&FamilyNameSize, nullptr);
	FamilyName.resize(FamilyNameSize - 1);
	GetCurrentPackageFamilyName(&FamilyNameSize, &FamilyName[0]);

	return FamilyName;
}

std::wstring UWP::Current::GetFullName()
{
	std::wstring FullName;
	uint32_t FullNameSize = 0;

	GetCurrentPackageFullName(&FullNameSize, nullptr);
	FullName.resize(FullNameSize - 1);
	GetCurrentPackageFullName(&FullNameSize, &FullName[0]);

	return FullName;
}

std::wstring UWP::Current::GetArchitecture()
{
	std::shared_ptr<PACKAGE_ID> PackageID = GetPackageIdentifier();
	if( PackageID )
	{
		switch( PackageID->processorArchitecture )
		{
		case APPX_PACKAGE_ARCHITECTURE_ARM:
		{
			return L"ARM";
			break;
		}
		case APPX_PACKAGE_ARCHITECTURE_X86:
		{
			return L"x86";
			break;
		}
		case APPX_PACKAGE_ARCHITECTURE_X64:
		{
			return L"x64";
			break;
		}
		case APPX_PACKAGE_ARCHITECTURE_NEUTRAL:
		{
			return L"Neutral";
			break;
		}
		}
	}
	return L"";
}

std::wstring UWP::Current::GetPublisher()
{
	std::shared_ptr<PACKAGE_ID> PackageID = GetPackageIdentifier();
	if( PackageID )
	{
		return std::wstring(PackageID->publisher);
	}
	return L"";
}

std::wstring UWP::Current::GetPublisherID()
{
	std::shared_ptr<PACKAGE_ID> PackageID = GetPackageIdentifier();
	if( PackageID )
	{
		return std::wstring(PackageID->publisherId);
	}
	return L"";
}

std::wstring UWP::Current::GetPackagePath()
{
	std::wstring Path;
	uint32_t PathSize = 0;

	GetCurrentPackagePath(&PathSize, nullptr);
	Path.resize(PathSize - 1);
	GetCurrentPackagePath(&PathSize, &Path[0]);

	return Path;
}

std::wstring UWP::Current::Storage::GetPublisherPath()
{
	std::shared_ptr<PACKAGE_ID> PackageID = GetPackageIdentifier();
	if( PackageID )
	{
		wchar_t UserPath[MAX_PATH] = { 0 };
		SHGetSpecialFolderPathW(nullptr, UserPath, CSIDL_PROFILE, false);

		std::wstring PublisherPath(UserPath);

		PublisherPath += L"\\AppData\\Local\\Publishers\\";
		PublisherPath += PackageID->publisherId;

		return PublisherPath;
	}
	return L"";
}

std::wstring UWP::Current::Storage::GetStoragePath()
{
	wchar_t UserPath[MAX_PATH] = { 0 };
	SHGetSpecialFolderPathW(nullptr, UserPath, CSIDL_PROFILE, false);

	std::wstring StoragePath(UserPath);

	StoragePath += L"\\AppData\\Local\\Packages\\" + UWP::Current::GetFamilyName();

	return StoragePath;
}

std::wstring UWP::Current::Storage::GetLocalCachePath()
{
	return GetStoragePath() + L"\\LocalCache";
}

std::wstring UWP::Current::Storage::GetLocalPath()
{
	return GetStoragePath() + L"\\LocalState";
}

std::wstring UWP::Current::Storage::GetRoamingPath()
{
	return GetStoragePath() + L"\\RoamingState";
}

std::wstring UWP::Current::Storage::GetTempStatePath()
{
	return GetStoragePath() + L"\\TempState";
}