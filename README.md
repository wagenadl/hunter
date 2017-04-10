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
	- Add bin64 (C:\Program Files\Point Grey Research\FlyCapture2\bin64) to PATH
- LibJpegTurbo 1.4.2 for Visual C++ 64-bit
- QT Open Source 5.6.2 Windows 64-bit, VS 2015
	- Install to C:\Qt\Qt5.6.2
	- Default installation options
	- Add QT bin (C:\Qt\Qt5.6.2\5.6\msvc2015_64\bin) to PATH
- Visual Studio 2015 Enterprise (Enterprise likely not required, just what I've been using)
- Windows 10 (again, 10 specifically likely not necessary, but what I've been using)

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
- In the same folder as Hunter.exe: FlyCapture2.dll, Qt5Core.dll, Qt5Gui.dll, Qt5Widgets.dll, platforms/qwindows.dll
- Windows 10

## Testing Procedure

We don't have any formal testing for this software, but here's details about the informal testing that I've been doing.
- Compatability with Matlab tools (through Piotr's toolbox) was tested by opening recorded videos in the seqViewer tool. This tool was downloaded from the Github repo at this version: https://github.com/pdollar/toolbox/tree/1a3c9869033548abb0c7a3c2aa6a7902c36f39c2. Confirmed all 4 recordings (PG, Color, Depth, IR) can be opened in a vanilla install of that toolbox with the compatability mode enabled.


## Notes

- Currently, we're only distributing the "compatable" version. In the future, we'll transition to the other version. When that happens, we should take another look at the format differences, and make sure that we can tell apart compatable and non-compatable videos by the header or something.
