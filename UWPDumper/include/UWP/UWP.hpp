#pragma once
#include <string>

// UWP Helper class
namespace UWP
{
namespace Current
{
std::wstring GetFamilyName();

std::wstring GetFullName();

std::wstring GetArchitecture();

std::wstring GetPublisher();

std::wstring GetPublisherID();

std::wstring GetPackagePath();

namespace Storage
{
/*
Apps from the same publisher may share files using the Publisher folder located at:
"%user_folder%\AppData\Local\Publishers\%PublisherID%\"
*/
std::wstring GetPublisherPath();

// Package Storage folders
std::wstring GetStoragePath();

std::wstring GetLocalPath();

std::wstring GetRoamingPath();

std::wstring GetTemporaryPath();
}
}
}
