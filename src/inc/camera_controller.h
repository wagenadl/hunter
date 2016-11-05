/** 
 * @file camera_controller.h
 * @brief Camera controller
 *
 * @author Santiago Navonne
 *
 */
#pragma once

// Libraries
#include <DepthSense.hxx>
#include <QTGui/QImage>
#include "FlyCapture2.h"

// C++
#include <array>

// Compatibility mode flag.
// If defined, forces 8-bit output and depth smoothing
// to maintain compatibility with old Matlab code.
// Also will .seqs with old-style headers.
#define COMPATIBILITY_MODE

class Streamer;

class CameraController
{
public:
    // Enums
    enum Cameras
    {
        PointGreyTop = 0,   /**< Top Point Grey Camera */
        PointGreyFront = 1, /**< Front Point Grey Camera */
        Color = 2,          /**< Color Camera */
        Depth = 3,          /**< Depth Camera */
        NUM_CAMERAS         /**< Number of cameras */
    };

    enum DepthSenseNodes
    {
        ColorNode = 0,       /**< DepthSense Color Node */
        DepthNode = 1,       /**< DepthSense Depth Node */
        NUM_DEPTHSENSE_NODES /**< Number of DepthSense nodes */
    };

    enum IntelChannels
    {
        Image = 0,          /**< Intel Color Channel */
        DepthMap = 1,       /**< Intel Depth Channel */
        IR = 2,             /**< Intel IR Channel */
        NUM_INTEL_CHANNELS  /**< Number of Intel channels */
    };

    enum CameraProperties
    {
        FPS = FlyCapture2::FRAME_RATE,        /**< Point Grey Frame Rate */
        Shutter = FlyCapture2::SHUTTER,       /**< Point Grey Shutter */
        Gain = FlyCapture2::GAIN,             /**< Point Grey Gain */
        Brightness = FlyCapture2::BRIGHTNESS, /**< Point Grey Brightness */
    };

	enum
	{
		PG_CAMERAS_MIN = 1,              /**< Minimum number of Point Grey cameras. */
		PG_CAMERAS_MAX = 2,              /**< Maximum number of Point Grey cameras. */
		INTEL_IMAGESTREAMPROFILEIDX = 0, /**< Intel camera setting: Image Stream Profile Index. */
		INTEL_DEPTHSTREAMPROFILEIDX = 0, /**< Intel camera setting: Depth Stream Profile Index. */
		INTEL_IMAGEBRIGHTNESS = 1000,    /**< Intel camera setting: Image Brightness. */
		INTEL_IMAGECONTRAST = 1000,      /**< Intel camera setting: Image Contrast. */
		INTEL_PRINTSTREAM = false,       /**< Intel camera setting: Print Stream. */
		INTEL_PRINTTIMING = false,       /**< Intel camera setting: Print Timing. */
		INTEL_SHOWCLOSEDPOINT = false    /**< Intel camera setting: Show Closed Point. */
	};

    // Structs

    /** Width and height of an acquired camera frame. */
    typedef struct _frame_size
    {
        int width;    /**< The width */
        int height;   /**< The height */
    } FrameSize;
    
    /** A Point Grey Camera */
    typedef struct _camera
    {
        FlyCapture2::Camera *Camera; /**< The instantiated Camera object. */
        double DefaultFPS;            /**< The default Frame Rate value. */
		double DefaultGain;           /**< The default Gain value. */
		double DefaultBrightness;     /**< The default Brightness value. */
		double DefaultShutter;        /**< The default Shutter value. */
        unsigned int DefaultHeight;  /**< The default frame height. */
        unsigned int DefaultWidth;   /**< The default frame width. */
    } Camera;

    // Function prototypes
    CameraController( void );
	~CameraController( void );

    int initPG();
    void initIntel( void(*depthTransporter)( DepthSense::DepthNode obj, DepthSense::DepthNode::NewSampleReceivedData data ),
                    void(*colorTransporter)( DepthSense::ColorNode obj, DepthSense::ColorNode::NewSampleReceivedData data ) );
    float getValue( Cameras cam, CameraProperties type );
	void setValue( Cameras cam, CameraProperties type, float val );

    DepthSense::Context getDepthSenseContext();
    static FrameSize getDepthSenseFormatSize( DepthSense::FrameFormat format );

	unsigned int numPGCameras;
	std::array< Camera, PG_CAMERAS_MAX + 2 > CameraProps; // Must use std::array due to CS2536

private:
    // Constants

    // Shared variables
    FlyCapture2::BusManager PGBusManager;

    DepthSense::Context depthSenseContext;
    DepthSense::Device  depthSenseDevice;
    DepthSense::Node    depthSenseNodes[ NUM_DEPTHSENSE_NODES ];
};