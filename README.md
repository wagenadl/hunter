# hunter

## Preparing for development

### Requirements
All requirement installers of proper version are hosted on the Dropbox, in the "Common" and "Development" folders.
- DepthSense SDK version 1.9.0.5, 64 bit, VS 2013
- FlyCap 2.10.3.169 SDK Windows 64-bit
	- Install to C:\Program Files\Point Grey Research\FlyCapture2\ (default)
	- "Complete"
	- Select USB (both Camera and Interface)
	- Select "register DLLs"
	- Add bin64 (C:\Program Files\Point Grey Research\FlyCapture2\bin64) to PATH ??? Maybe
- LibJpegTurbo 1.4.2 for Visual C++ 64-bit
- QT Open Source 5.6.2 Windows 64-bit, VS 2015
	- Install to C:\Qt\Qt5.6.2
	- Default installation options
	- Add QT bin (C:\Qt\Qt5.6.2\5.6\msvc2015_64\bin) to PATH
- Visual Studio 2015 Enterprise
- Windows 10

### Other instructions

- Build the qt files in qt/. View the readme for more info. 
- Upon starting Visual Studio (by opening the .sln file), select 64-bit build configuration.
- Select either Debug or Release config, depending on requirements.
- To mark version numbers, for now we're just iterating the number that appears in the program title. This can be edited in hunter.ui, line 20. Remember to re-run the .bat file after this change.

## Installing on a end-user computer

### Requirements

All requirement installers of proper version are hosted on the Dropbox, in the "Common" and "Release" folders.
- DepthSense SDK version 1.9.0.5, 64 bit, VS 2013
- FlyCap 2.10.3.169 Viewer Windows 64-bit
- LibJpegTurbo 1.4.2 for Visual C++ 64-bit (not sure if necessary)
- QT???
- Windows 10

