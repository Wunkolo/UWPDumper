# UWPDumper [![Build status](https://ci.appveyor.com/api/projects/status/ys3yvpv0bdel15sx?svg=true)](https://ci.appveyor.com/project/Wunkolo/uwpdumper) [![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/Wunkolo/UWPDumper/master/LICENSE)
DLL and Injector for dumping UWP applications at run-time to bypass encrypted file system protection.

![image](https://cloud.githubusercontent.com/assets/644247/23590473/a06ee446-0195-11e7-92b7-909177b6485a.gif)


Run `UWPDumper.exe` and enter valid UWP Process ID to inject into.
App file system will be dumped into:

`C:\Users\(Username)\AppData\Local\Packages\(Package Family Name)\TempState\DUMP`

UWPDumper requires the [Windows 10 Anniversary SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk)(10.0.14393.0) to be compiled.

## [Download latest x64 binary here](https://ci.appveyor.com/project/Wunkolo/uwpdumper/build/artifacts)