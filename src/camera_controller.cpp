/** 
 * @file camera_controller.cpp
 * @brief Camera controller
 *
 * @author Santiago Navonne
 *
 */

// Project includes
#include "camera_controller.h"
#include "exceptions.h"

// Libraries
#include <QTCore/QtDebug>
#include <QTGui/QImage>
#include "FlyCapture2.h"
#include <DepthSense.hxx>

// C++
#include <stdio.h>
#include <iostream> //cout

#include <chrono>
#include <time.h>

using namespace std;

// Runtime constants

/**
 * @brief Camera controller constructor
 * @arg void
 */                                                          //  *PG   FPS  Gain   Bright Shutter Height Width
CameraController::CameraController( void ) : CameraProps ( { { { NULL, 30.00,  4.0, 9.204, 19.871, 1200, 1920 }, // Point Grey Top
                                                               { NULL, 30.00, 10.0, 7.886, 19.948, 1200, 1920 }, // Point Gret Front
															   { NULL, 30.00, 0.0, 0.0, 0.0, 720, 1280 }, // Color
															   { NULL, 30.00, 0.0, 0.0, 0.0, 240, 320 }, // Depth 
                                                           } } ) // Awkward syntax due to CS2536
{
}
        
/**
 * @brief Camera controller destructor
 * @arg void
 */
CameraController::~CameraController( void )
{
    FlyCapture2::Error err;

    if ( depthSenseContext.isSet() )
        depthSenseContext.stopNodes();

    for ( int node = 0; node < NUM_DEPTHSENSE_NODES; node++ )
    {
        if ( depthSenseNodes[ node ].isSet() )
            depthSenseContext.unregisterNode( depthSenseNodes[ node ] );
    }


    // Clean up after all the PG cameras
	for ( unsigned int i = 0; i < numPGCameras; i++ )
    {
        if ( CameraProps[ i ].Camera == NULL )
            continue;

        err = CameraProps[ i ].Camera->StopCapture();
        if ( err != FlyCapture2::PGRERROR_OK )
		{
#ifdef DEBUG
		    qDebug() << "Point Grey init error: " << err.GetDescription() << endl;
#endif
            delete CameraProps[ i ].Camera;
			continue;
		}

        err = CameraProps[ i ].Camera->Disconnect();
        if ( err != FlyCapture2::PGRERROR_OK )
		{
#ifdef DEBUG
		    qDebug() << "Point Grey init error: " << err.GetDescription() << endl;
#endif
            delete CameraProps[ i ].Camera;
			continue;
		}
        delete CameraProps[ i ].Camera;
    }
}

/**
 * @brief Initialize the Point Grey cameras.
 * @arg None.
 * @returns The number of PG cameras initialized
 */
int CameraController::initPG()
{
	FlyCapture2::Error err;
	FlyCapture2::Format7ImageSettings imageSettings;
	bool validSettings;
	FlyCapture2::Format7PacketInfo packetInfo;
	int camerasInitialized = 0;

	// Try to get the number of Point Grey cameras
	err = PGBusManager.GetNumOfCameras(&numPGCameras);
	if (err != FlyCapture2::PGRERROR_OK)
	{
#ifdef DEBUG
		qDebug() << "Point Grey init error: " << err.GetDescription() << endl;
#endif
		throw EXCEPTION_PG;
		return 0;
	}

#ifdef DEBUG
	qDebug() << "Number of PG cameras detected: " << numPGCameras << endl;
#endif

	// Don't take more than two cameras
	if (numPGCameras > PG_CAMERAS_MAX)
		numPGCameras = PG_CAMERAS_MAX;

	// Connect to all detected cameras and attempt to set them to
	// a common video mode and frame rate
	for (unsigned int i = 0; i < numPGCameras; i++)
	{
		// Instantiate i'th camera
		CameraProps[i].Camera = new FlyCapture2::Camera();

		// Identify the camera
		FlyCapture2::PGRGuid guid;
		err = PGBusManager.GetCameraFromIndex(i, &guid);
		if (err != FlyCapture2::PGRERROR_OK)
		{
#ifdef DEBUG
			qDebug() << "Point Grey init error:" << err.GetDescription() << endl;
#endif
			continue;
		}

		// Connect to the camera
		err = CameraProps[i].Camera->Connect(&guid);
		if (err != FlyCapture2::PGRERROR_OK)
		{
#ifdef DEBUG
			qDebug() << "Point Grey init error:" << err.GetDescription() << endl;
#endif
			continue;
		}

		// Set default values
		try
		{
			setValue((Cameras)i, CameraProperties::FPS, CameraProps[i].DefaultFPS);
		}
		catch (exception_t e)
		{
			if (e == EXCEPTION_PG_CONFIG); // TODO
			if (e == EXCEPTION_PG_INVALID_CAM); // TODO
		}

		try
		{
			setValue((Cameras)i, CameraProperties::Shutter, CameraProps[i].DefaultShutter);
		}
		catch (exception_t e)
		{
			if (e == EXCEPTION_PG_CONFIG); // TODO
			if (e == EXCEPTION_PG_INVALID_CAM); // TODO
		}

		try
		{
			setValue((Cameras)i, CameraProperties::Gain, CameraProps[i].DefaultGain);
		}
		catch (exception_t e)
		{
			if (e == EXCEPTION_PG_CONFIG); // TODO
			if (e == EXCEPTION_PG_INVALID_CAM); // TODO
		}

		try
		{
			setValue((Cameras)i, CameraProperties::Brightness, CameraProps[i].DefaultBrightness);
		}
		catch (exception_t e)
		{
			if (e == EXCEPTION_PG_CONFIG); // TODO
			if (e == EXCEPTION_PG_INVALID_CAM); // TODO
		}

		// Set streaming configuration
		imageSettings.height = CameraProps[i].DefaultHeight;
		imageSettings.width = CameraProps[i].DefaultWidth;
		imageSettings.pixelFormat = FlyCapture2::PixelFormat::PIXEL_FORMAT_MONO8;
		err = CameraProps[i].Camera->ValidateFormat7Settings(&imageSettings, &validSettings, &packetInfo);
		if (err != FlyCapture2::PGRERROR_OK)
		{
#ifdef DEBUG
			qDebug() << "Point Grey init error:" << err.GetDescription() << endl;
#endif
			continue;
		}
		err = CameraProps[i].Camera->SetFormat7Configuration(&imageSettings, packetInfo.recommendedBytesPerPacket);
		if (err != FlyCapture2::PGRERROR_OK)
		{
#ifdef DEBUG
			qDebug() << "Point Grey init error:" << err.GetDescription() << endl;
#endif
			continue;
		}

		camerasInitialized++;
	}
	return camerasInitialized;
}

/**
 * @brief Initialize the intel camera.
 * this looks good - similar to what i wrote except a missing onDisconnect event handler - zack
 * @arg None.
 * @returns void
 */
void CameraController::initIntel( void(*depthTransporter)( DepthSense::DepthNode obj, DepthSense::DepthNode::NewSampleReceivedData data ),
                                  void(*colorTransporter)( DepthSense::ColorNode obj, DepthSense::ColorNode::NewSampleReceivedData data ) )
{
    try {
        // Create context
        depthSenseContext = DepthSense::Context::create( "localhost" );

        // Find device
        vector<DepthSense::Device> devices = depthSenseContext.getDevices();
        if ( !devices.empty() )
            depthSenseDevice = devices[ 0 ];
        else
            throw EXCEPTION_INTEL; 

        // Identify nodes
        vector<DepthSense::Node> nodes = depthSenseDevice.getNodes();
        for ( auto& iter : nodes )
        {
            DepthSense::Node node = iter;

            if ( node.is<DepthSense::ColorNode>() )
            {
                DepthSense::ColorNode colorNode = node.as<DepthSense::ColorNode>();
                depthSenseContext.registerNode( colorNode );
                colorNode.setEnableColorMap( true );
                colorNode.newSampleReceivedEvent().connect( colorTransporter );

                DepthSense::ColorNode::Configuration config = colorNode.getConfiguration();
                config.frameFormat = DepthSense::FrameFormat::FRAME_FORMAT_VGA;
                config.compression = DepthSense::CompressionType::COMPRESSION_TYPE_MJPEG;
                config.powerLineFrequency = DepthSense::PowerLineFrequency::POWER_LINE_FREQUENCY_50HZ;
                config.framerate = CameraProps[Cameras::Color].DefaultFPS;

                depthSenseContext.requestControl( colorNode, 0 );
                colorNode.setConfiguration( config );
				depthSenseContext.releaseControl(colorNode);

                depthSenseNodes[ ColorNode ] = node;
            }
            else if ( node.is<DepthSense::DepthNode>() )
            {
                DepthSense::DepthNode depthNode = node.as<DepthSense::DepthNode>();
                depthSenseContext.registerNode( node );
                depthNode.setEnableDepthMap( true );
				depthNode.setEnableConfidenceMap(true);
                depthNode.newSampleReceivedEvent().connect( depthTransporter );



                DepthSense::DepthNode::Configuration config = depthNode.getConfiguration();
                config.frameFormat = DepthSense::FrameFormat::FRAME_FORMAT_QVGA;
                config.framerate = CameraProps[Cameras::Depth].DefaultFPS;;
                config.mode = DepthSense::DepthNode::CAMERA_MODE_CLOSE_MODE;
                config.saturation = true;

				
                
                depthSenseContext.requestControl( depthNode, 0 );

#ifdef COMPATIBILITY_MODE

				// If we're in compatibility mode, enable smoothing filter
				// Filter 8 is mostly unnecessary, only affects regions outside the ROI

				depthNode.setEnableFilter1(true);
				depthNode.setEnableFilter8(true);
				depthNode.setEnableFilter9(true);
				
				// Set the parameters arbitrarily...

				depthNode.setFilter1Parameter1(10000);
				depthNode.setFilter1Parameter2(2500);
				depthNode.setFilter1Parameter3(120);
				depthNode.setFilter1Parameter4(500);

				// 9.1 has a huge impact on CPU: setting to 100 doubles usage vs 10. 10 vs 1 seems minimal
				depthNode.setFilter9Parameter1(10);
				depthNode.setFilter9Parameter2(100);
				depthNode.setFilter9Parameter3(10);
				depthNode.setFilter9Parameter4(2);

				depthNode.setFilter8Parameter1(450);

#endif


                depthNode.setConfiguration( config );
				depthSenseContext.releaseControl(depthNode);

                depthSenseNodes[ DepthNode ] = node;
            }
        }

        // Verify that both nodes were successfully identified
        if ( !depthSenseNodes[ ColorNode ].isSet() || !depthSenseNodes[ DepthNode ].isSet() )
            throw EXCEPTION_INTEL;

        depthSenseContext.startNodes();
    }
    catch ( ... )
    {
#ifdef DEBUG
			qDebug() << "Intel camera initialization error." << endl;
#endif
        throw EXCEPTION_INTEL;
        return;
    }

}

/**
 * @brief Mutator for a camera's configuration value.
 * @param cam The camera whose attribute should be changed
 * @param type The attribute to be changed
 * @param val The new value for the property
 * @returns void
 */
void CameraController::setValue( Cameras cam, CameraProperties type, float val )
{
	FlyCapture2::Property camProp;
    FlyCapture2::Error err;

    // Check that the camera is valid
	if ( cam >= numPGCameras ) {
        qDebug() << "Invalid PG camera configured. Configured property " << (int)type << " for camera " << (int)cam << endl;
        throw EXCEPTION_PG_INVALID_CAM;
        return;
    }

    // Configure values
    camProp.type = ( FlyCapture2::PropertyType ) type;
	camProp.absValue = val;
	camProp.absControl = true;
	camProp.onePush = false;
	camProp.onOff = true;
	camProp.autoManualMode = false;

    // And apply them
    err = CameraProps[ cam ].Camera->SetProperty( &camProp );
	if ( err != FlyCapture2::PGRERROR_OK )
	{
#ifdef DEBUG
	    qDebug() << "Point Grey setValue error:" << err.GetDescription() << endl;
#endif
        throw EXCEPTION_PG_CONFIG;
        return;
	}
}


/**
 * @brief Accessor for a camera's configuration value.
 * @param cam The camera whose attribute should be returned
 * @param type The attribute to be accessed
 * @returns The requested configuration value
 */
float CameraController::getValue( Cameras cam, CameraProperties type )
{
    FlyCapture2::Property attribute;
    FlyCapture2::Error err;

    // Check that the camera is valid
	if ( cam < numPGCameras ) {
        // And get the configured property value
		attribute.type = (FlyCapture2::PropertyType) type;
        err = CameraProps[ cam ].Camera->GetProperty( &attribute );
		if ( err != FlyCapture2::PGRERROR_OK )
		{
#ifdef DEBUG
	        qDebug() << "Point Grey getValue error:" << err.GetDescription() << endl;
#endif
            throw EXCEPTION_PG_CONFIG;
            return 0.0;
		}

        // Return the property value
		return attribute.absValue;
	}
    qDebug() << "Invalid PG camera configured. Requested property " << (int)type << " from camera " << (int)cam << endl;
    throw EXCEPTION_PG_INVALID_CAM;
	return 0.0;
}

/**
 * @brief Accessor for the DepthSense context object.
 * @returns The instance's DepthSense Context object.
 */
DepthSense::Context CameraController::getDepthSenseContext()
{
    return depthSenseContext;
}

/**
 * @brief DepthSense FrameFormat to width and height translator.
 * @param format The FrameFormat.
 * @returns The corresponding FrameSize.
 */
CameraController::FrameSize CameraController::getDepthSenseFormatSize( DepthSense::FrameFormat format )
{
    FrameSize frameSize;
    switch ( format )
    {
    case DepthSense::FrameFormat::FRAME_FORMAT_QQVGA:
        frameSize.width = 160;
        frameSize.height = 120;
        break;
    case DepthSense::FrameFormat::FRAME_FORMAT_QCIF:
        frameSize.width = 176;
        frameSize.height = 144;
        break;
    case DepthSense::FrameFormat::FRAME_FORMAT_HQVGA:
        frameSize.width = 240;
        frameSize.height = 160;
        break;
    case DepthSense::FrameFormat::FRAME_FORMAT_QVGA:
        frameSize.width = 320;
        frameSize.height = 240;
        break;
    case DepthSense::FrameFormat::FRAME_FORMAT_CIF:
        frameSize.width = 352;
        frameSize.height = 288;
        break;
    case DepthSense::FrameFormat::FRAME_FORMAT_HVGA:
        frameSize.width = 480;
        frameSize.height = 320;
        break;
    default:
    case DepthSense::FrameFormat::FRAME_FORMAT_VGA:
        frameSize.width = 640;
        frameSize.height = 480;
        break;
    case DepthSense::FrameFormat::FRAME_FORMAT_WXGA_H:
        frameSize.width = 1280;
        frameSize.height = 720;
        break;
    case DepthSense::FrameFormat::FRAME_FORMAT_DS311:
        frameSize.width = 320;
        frameSize.height = 120;
        break;
    case DepthSense::FrameFormat::FRAME_FORMAT_XGA:
        frameSize.width = 768;
        frameSize.height = 1024;
        break;
    case DepthSense::FrameFormat::FRAME_FORMAT_SVGA:
        frameSize.width = 800;
        frameSize.height = 600;
        break;
    case DepthSense::FrameFormat::FRAME_FORMAT_OVVGA:
        frameSize.width = 636;
        frameSize.height = 480;
        break;
    case DepthSense::FrameFormat::FRAME_FORMAT_WHVGA:
        frameSize.width = 640;
        frameSize.height = 240;
        break;
    case DepthSense::FrameFormat::FRAME_FORMAT_NHD:
        frameSize.width = 640;
        frameSize.height = 360;
        break;
    }
    return frameSize;
}
