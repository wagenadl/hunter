# hunter

Preparing for development:

Make sure you have the development SDK's installed for the PointGrey cameras, the DepthSense cameras,
libjpegturbo, and QT. 

You need to build the qt files in qt/. View the readme for more info. 

You may need to manually set the C++ include directories, and the linker directories, for depthsense, PointGrey, QT, and libjpegturbo.

We want a 64 bit build config, may need to set this also.

To mark version numbers, for now we're just iterating the number that appears in the program title. This can be edited in hunter.ui, line 20. Remember to re-run the .bat file after this change.


There is (in camera_controller.h) a COMPATIBILITY_MODE flag. If this flag is set, the code will be built in compatability mode. This applies depth smoothing, saves everything in 8-bit color, and enables the matlab-style .seq format. 