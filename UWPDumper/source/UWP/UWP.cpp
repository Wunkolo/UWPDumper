#include <UWP/UWP.hpp>

#include <Windows.h>
#include <ShlObj.h>
#include <appmodel.h>
#include <AppxPackaging.h>

#include <windows.storage.h>
#include <Windows.ApplicationModel.h>
#include <wrl.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#include <memory>

namespace
{
ComPtr<ABI::Windows::Storage::IApplicationData> GetIApplicationData()
{
	ComPtr<ABI::Windows::Storage::IApplicationDataStatics> AppDataStatics;

	if(
		RoGetActivationFactory(
			HStringReference(RuntimeClass_Windows_Storage_ApplicationData).Get(),
			__uuidof(AppDataStatics), &AppDataStatics
		) < 0
	)
	{
		// Error getting ApplicationData statics
	}

	ComPtr<ABI::Windows::Storage::IApplicationData> AppData;
	if( AppDataStatics->get_Current(&AppData) < 0 )
	{
		// Error getting current IApplicationData
	}

	return AppData;
}

ComPtr<ABI::Windows::ApplicationModel::IPackage> GetCurrentPackage()
{
	ComPtr<ABI::Windows::ApplicationModel::IPackageStatics> PackageStatics;
	if(
		RoGetActivationFactory(
			HStringReference(RuntimeClass_Windows_ApplicationModel_Package).Get(),
			__uuidof(PackageStatics), &PackageStatics
		) < 0
	)
	{
		// Error getting package statics
		return nullptr;
	}

	ComPtr<ABI::Windows::ApplicationModel::IPackage> CurrentPackage;
	if( PackageStatics->get_Current(&CurrentPackage) )
	{
		// Error getting current package
	}

	return CurrentPackage;
}

ComPtr<ABI::Windows::ApplicationModel::IPackageId> GetCurrentPackageID()
{
	ComPtr<ABI::Windows::ApplicationModel::IPackage> CurrentPackage =
		GetCurrentPackage();

	ComPtr<ABI::Windows::ApplicationModel::IPackageId> CurrentPackageID;
	if( CurrentPackage->get_Id(&CurrentPackageID) < 0 )
	{
		// error getting current package ID
		return nullptr;
	}
	return CurrentPackageID;
}

template< typename T >
void FreeDeleter(T* Data)
{
	free(Data);
}

std::unique_ptr<PACKAGE_ID, decltype(&FreeDeleter<PACKAGE_ID>)> GetPackageIdentifier()
{
	std::uint32_t Size = 0;
	GetCurrentPackageId(&Size, nullptr);

	if( Size )
	{
		std::unique_ptr<PACKAGE_ID, decltype(&FreeDeleter<PACKAGE_ID>)> PackageID(
			reinterpret_cast<PACKAGE_ID*>(malloc(Size)),
			FreeDeleter<PACKAGE_ID>
		);
		GetCurrentPackageId(
			&Size,
			reinterpret_cast<std::uint8_t*>(PackageID.get())
		);
		return PackageID;
	}
	return std::unique_ptr<PACKAGE_ID, decltype(&FreeDeleter<PACKAGE_ID>)>(
		nullptr,
		FreeDeleter<PACKAGE_ID>
	);
}

std::unique_ptr<PACKAGE_INFO, decltype(&FreeDeleter<PACKAGE_INFO>)> GetPackageInfo()
{
	std::uint32_t Size = 0;
	std::uint32_t Count = 0;
	GetCurrentPackageInfo(PACKAGE_FILTER_HEAD, &Size, nullptr, &Count);

	if( Size )
	{
		std::unique_ptr<PACKAGE_INFO, decltype(&FreeDeleter<PACKAGE_INFO>)> PackageInfo(
			reinterpret_cast<PACKAGE_INFO*>(malloc(Size)),
			FreeDeleter<PACKAGE_INFO>
		);

		GetCurrentPackageInfo(
			PACKAGE_FILTER_HEAD,
			&Size,
			reinterpret_cast<std::uint8_t*>(PackageInfo.get()),
			&Count
		);
		return PackageInfo;
	}

	return std::unique_ptr<PACKAGE_INFO, decltype(&FreeDeleter<PACKAGE_INFO>)>(
		nullptr,
		FreeDeleter<PACKAGE_INFO>
	);
}
}

std::wstring UWP::Current::GetFamilyName()
{
	ComPtr<ABI::Windows::ApplicationModel::IPackageId> PackageID = GetCurrentPackageID();

	HString FamilyNameString;

	if( PackageID->get_FamilyName(FamilyNameString.GetAddressOf()) < 0 )
	{
		// Failed to get string
		return L"";
	}

	std::uint32_t StringLength;
	const wchar_t* FamilyNameRaw = FamilyNameString.GetRawBuffer(&StringLength);
	return std::wstring(FamilyNameRaw, StringLength);
}

std::wstring UWP::Current::GetFullName()
{
	ComPtr<ABI::Windows::ApplicationModel::IPackageId> PackageID = GetCurrentPackageID();

	HString FullNameString;

	if( PackageID->get_FullName(FullNameString.GetAddressOf()) < 0 )
	{
		// Failed to get string
		return L"";
	}

	std::uint32_t StringLength;
	const wchar_t* FullNameRaw = FullNameString.GetRawBuffer(&StringLength);
	return std::wstring(FullNameRaw, StringLength);
}

std::wstring UWP::Current::GetArchitecture()
{
	const auto PackageID = GetCurrentPackageID();
	if( PackageID )
	{
		ABI::Windows::System::ProcessorArchitecture Arch;
		PackageID->get_Architecture(&Arch);
		switch( Arch )
		{
		case ABI::Windows::System::ProcessorArchitecture::ProcessorArchitecture_Arm:
		{
			return L"ARM";
		}
		case ABI::Windows::System::ProcessorArchitecture::ProcessorArchitecture_X86:
		{
			return L"x86";
		}
		case ABI::Windows::System::ProcessorArchitecture::ProcessorArchitecture_X64:
		{
			return L"x64";
		}
		case ABI::Windows::System::ProcessorArchitecture::ProcessorArchitecture_Neutral:
		{
			return L"Neutral";
		}
		}
	}
	return L"Unknown";
}

std::wstring UWP::Current::GetPublisher()
{
	ComPtr<ABI::Windows::ApplicationModel::IPackageId> PackageID = GetCurrentPackageID();

	HString PublisherString;

	if( PackageID->get_Publisher(PublisherString.GetAddressOf()) < 0 )
	{
		// Failed to get string
		return L"";
	}

	std::uint32_t StringLength;
	const wchar_t* PublisherRaw = PublisherString.GetRawBuffer(&StringLength);
	return std::wstring(PublisherRaw, StringLength);
}

std::wstring UWP::Current::GetPublisherID()
{
	ComPtr<ABI::Windows::ApplicationModel::IPackageId> PackageID = GetCurrentPackageID();

	HString PublisherID;

	if( PackageID->get_PublisherId(PublisherID.GetAddressOf()) < 0 )
	{
		// Failed to get string
		return L"";
	}

	std::uint32_t StringLength;
	const wchar_t* PublisherIDRaw = PublisherID.GetRawBuffer(&StringLength);
	return std::wstring(PublisherIDRaw, StringLength);
}

std::wstring UWP::Current::GetPackagePath()
{
	auto Package = GetCurrentPackage();

	ComPtr<ABI::Windows::Storage::IStorageFolder> InstallLocationFolder;

	if( Package->get_InstalledLocation(&InstallLocationFolder) < 0 )
	{
		// Failed to get folder
		return L"";
	}

	ComPtr<ABI::Windows::Storage::IStorageItem> FolderItem;
	if( InstallLocationFolder.As(&FolderItem) < 0 )
	{
		// Failed to cast to IStorageItem
		return L"";
	}

	HString PackagePathString;

	if( FolderItem->get_Path(PackagePathString.GetAddressOf()) < 0 )
	{
		// Failed to get path as string
		return L"";
	}

	std::uint32_t PathLength;
	const wchar_t* PackagePathRaw = PackagePathString.GetRawBuffer(&PathLength);
	return std::wstring(PackagePathRaw, PathLength);
}

std::wstring UWP::Current::Storage::GetPublisherPath()
{
	wchar_t UserPath[MAX_PATH] = {
		0
	};
	SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, SHGFP_TYPE_CURRENT, UserPath);

	std::wstring PublisherPath(UserPath);

	PublisherPath += L"\\AppData\\Local\\Publishers\\";
	PublisherPath += GetPublisherID();

	return PublisherPath;
}

std::wstring UWP::Current::Storage::GetStoragePath()
{
	wchar_t UserPath[MAX_PATH] = {
		0
	};
	SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, SHGFP_TYPE_CURRENT, UserPath);

	std::wstring StoragePath(UserPath);

	StoragePath += L"\\AppData\\Local\\Packages\\" + GetFamilyName();

	return StoragePath;
}

std::wstring UWP::Current::Storage::GetLocalPath()
{
	ComPtr<ABI::Windows::Storage::IApplicationData> AppData = GetIApplicationData();

	ComPtr<ABI::Windows::Storage::IStorageFolder> LocalStateFolder;

	if( AppData->get_LocalFolder(&LocalStateFolder) < 0 )
	{
		// Failed to get folder
		return L"";
	}

	ComPtr<ABI::Windows::Storage::IStorageItem> FolderItem;
	if( LocalStateFolder.As(&FolderItem) < 0 )
	{
		// Failed to cast to IStorageItem
		return L"";
	}

	HString LocalPathString;

	if( FolderItem->get_Path(LocalPathString.GetAddressOf()) < 0 )
	{
		// Failed to get path as string
		return L"";
	}

	std::uint32_t PathLength;
	const wchar_t* LocalPathRaw = LocalPathString.GetRawBuffer(&PathLength);
	return std::wstring(LocalPathRaw, PathLength);
}

std::wstring UWP::Current::Storage::GetRoamingPath()
{
	ComPtr<ABI::Windows::Storage::IApplicationData> AppData = GetIApplicationData();

	ComPtr<ABI::Windows::Storage::IStorageFolder> RoamingFolder;

	if( AppData->get_RoamingFolder(&RoamingFolder) < 0 )
	{
		// Failed to get folder
		return L"";
	}

	ComPtr<ABI::Windows::Storage::IStorageItem> FolderItem;
	if( RoamingFolder.As(&FolderItem) < 0 )
	{
		// Failed to cast to IStorageItem
		return L"";
	}

	HString RoamingPathString;

	if( FolderItem->get_Path(RoamingPathString.GetAddressOf()) < 0 )
	{
		// Failed to get path as string
		return L"";
	}

	std::uint32_t PathLength;
	const wchar_t* RoamingPathRaw = RoamingPathString.GetRawBuffer(&PathLength);
	return std::wstring(RoamingPathRaw, PathLength);
}

std::wstring UWP::Current::Storage::GetTemporaryPath()
{
	ComPtr<ABI::Windows::Storage::IApplicationData> AppData = GetIApplicationData();

	ComPtr<ABI::Windows::Storage::IStorageFolder> TemporaryFolder;

	if( AppData->get_TemporaryFolder(&TemporaryFolder) < 0 )
	{
		// Failed to get folder
		return L"";
	}

	ComPtr<ABI::Windows::Storage::IStorageItem> FolderItem;
	if( TemporaryFolder.As(&FolderItem) < 0 )
	{
		// Failed to cast to IStorageItem
		return L"";
	}

	HString TemporaryPathString;

	if( FolderItem->get_Path(TemporaryPathString.GetAddressOf()) < 0 )
	{
		// Failed to get path as string
		return L"";
	}

	std::uint32_t PathLength;
	const wchar_t* TemporyPathRaw = TemporaryPathString.GetRawBuffer(&PathLength);
	return std::wstring(TemporyPathRaw, PathLength);
}
