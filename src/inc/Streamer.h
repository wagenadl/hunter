/** 
 * @file streamer.h
 * @brief UI video streamer
 *
 * @author Santiago Navonne
 * TODO
 */

#pragma once

// Libraries
#include <DepthSense.hxx>
#include <Pointer.hxx>

#include <QTCore/qt_windows.h>
#include <QTGui/QImage>
#include <QTCore/qrunnable.h>
#include <QTCore/qdebug.h>
#include <QTCore/qthread.h>
#include <QTGui/qcolor.h>

// C++
#include <iostream>
#include <stdint.h>
#include <time.h>
#include <string>
#include <chrono>
#include <queue>
#include <iomanip>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>

// Project includes
#include "camera_controller.h"

using namespace std;

#define CLAMP(f, b)  if      ( f > 255 ) b = 255; \
                     else if ( f < 0 )   b = 0;   \
                     else                b = (uchar)f;



class SEQWriter;

class CameraFrame
{
public:

    DepthSense::Pointer<uint8_t> DSData8;  /**< 8-bit frame data (color). */
    DepthSense::Pointer<int16_t> DSData16; /**< 16-bit frame data (grayscale). */
	DepthSense::Pointer<int16_t> DSConfidenceMap; /**< 16-bit confidence map. */
    DepthSense::FrameFormat imageFormat;   /**< Format of the acquired image. */


    FlyCapture2::Image *PGData;            /**< Point Grey frame data. */
    int timestampSeconds;                  /**< Timestamp: seconds value. */
    int timestampMilliSeconds;             /**< Timestamp: milliseconds value. */
    CameraFrame() { };

    CameraFrame( DepthSense::Pointer<uint8_t> data, DepthSense::FrameFormat format, int sec, int ms ) 
               : DSData8( data ), imageFormat( format ), PGData( NULL ), timestampSeconds( sec ), timestampMilliSeconds( ms ) { };
    CameraFrame( DepthSense::Pointer<int16_t> data, DepthSense::FrameFormat format, int sec, int ms ) 
               : DSData16( data ), imageFormat( format ), PGData( NULL ), timestampSeconds( sec ), timestampMilliSeconds( ms ) { };
	CameraFrame(DepthSense::Pointer<int16_t> data, DepthSense::Pointer<int16_t> confidence_map, DepthSense::FrameFormat format, int sec, int ms)
		: DSData16(data), imageFormat(format), PGData(NULL), timestampSeconds(sec), timestampMilliSeconds(ms), DSConfidenceMap(confidence_map) {};

    CameraFrame( FlyCapture2::Image *data, int sec, int ms ) 
               : PGData( data ), timestampSeconds( sec ), timestampMilliSeconds( ms ) { };

	~CameraFrame(void) {
		if (PGData) {
			delete PGData;
		}
	}
};

class FrameQueue
{
public:
    std::queue<CameraFrame*> queue;       /**< The actual queue. */
    std::mutex mutex;                     /**< The queue's synchronization mutex. */

    void push( CameraFrame *data );
    CameraFrame* pop();
};
/*
class SingleFrameBuffer
{
public:
	CameraFrame* current_frame; // Buffer that holds a single frame
	std::mutex mutex;			// Access control mutex

	SingleFrameBuffer() :current_frame(nullptr) {}
	void push(CameraFrame* frame);
};
*/
class SynchronizationQueue
{
public:
	static const int max_synchronization_queue_size = 2;    /**< Max size queue can grow */
	std::mutex mutex;			// Access control mutex
	std::queue<CameraFrame*> current_frame_queue;
	void push(CameraFrame* frame);
};

class Streamer : public QThread
{
	Q_OBJECT

public:
    // Constants
    enum
    {
       JPEG_QUALITY = 70                /**< JPEG Compression quality (0-100). */
    };
    enum Channels
    {
        PointGreyTop = CameraController::Cameras::PointGreyTop,     /**< The Point Grey Top camera channel. */
        PointGreyFront = CameraController::Cameras::PointGreyFront, /**< The Point Grey Front camera channel. */
        Color = CameraController::Cameras::Color,                   /**< The Color camera channel. */
        Depth = CameraController::Cameras::Depth,                   /**< The Depth camera channel. */
        IR,                                                         /**< The IR channel. */
    };
    static const int N_CHANNELS = 5;                                /**< Number of channels. */
	static const int MAX_QUEUE_SIZE = 5;                            /**< Max size frame queue can grow */
	static const int UI_UPDATE_RATE = 100;                            /**< Update rate, in ms, of the UI, per channel */
	

    enum ROICoordinates
    {
        X,       /**< Region-of-Interest X coordinate. */
        Y,       /**< Region-of-Interest Y coordinate. */
        W,       /**< Region-of-Interest width. */
        H,       /**< Region-of-Interest height. */
        ROI_SIZE /**< Number of items per ROI. */
    };

	//Constructor
	Streamer( CameraController* _camera );
	//Destructor
	~Streamer(void);

	//check if the player is streamimg
	bool isStopped();

	// Set working dir
	void setCurrentWorkingDir(std::string workingDir);

	int maxDepthMM;

	// Control booleans
	bool record;
	bool isPGswitched;

	void stopRecording();
    void startRecording(bool pgt, bool pgf, bool color, bool depth);

    void startStreaming( CameraController::Cameras camera );
    void stopStreaming( CameraController::Cameras camera );

    void setROI( CameraController::Cameras camera, int x, int y, int w, int h );
    void setCompressed( CameraController::Cameras camera, bool compressed );
    void saveSnapshot( CameraController::Cameras camera );

    int getROI( CameraController::Cameras camera, ROICoordinates value );
	int getOriginalROI(CameraController::Cameras camera, ROICoordinates value);

    void run();


    static Streamer *transporterObject;

	void Streamer::checkFrameBuffer(bool p_pgTop, bool p_pgFront, bool p_depth, bool p_color);

	static void PGWrapper(FlyCapture2::Image* pImage, const void* pCallbackData)
	{
		transporterObject->PGImageTransporter(pImage, pCallbackData);
	}

    static void depthSenseColorWrapper( DepthSense::ColorNode node, DepthSense::ColorNode::NewSampleReceivedData data )
    {
        transporterObject->depthSenseColorTransporter( data );
    }
    static void depthSenseDepthWrapper( DepthSense::DepthNode node, DepthSense::DepthNode::NewSampleReceivedData data )
    {
        transporterObject->depthSenseDepthTransporter( data );
    }
    void depthSenseColorTransporter( DepthSense::ColorNode::NewSampleReceivedData data );
    void depthSenseDepthTransporter( DepthSense::DepthNode::NewSampleReceivedData data );
	void imageTransporter();


signals:
	void updateCamera( CameraController::Cameras, QImage );

	void onStopSavingEvent();

private:
    // Constants
    enum 
    {
        FRAME_RATE_DEFAULT = 30, /**< Default frame rate for all cameras. */
        MAX_DEPTH_DEFAULT = 480, /**< Default depth value of the background. */
    };

    /** Attributes for a stream */
    struct stream_attributes
    {
        bool recording;           /**< Is the stream recording? */
        bool streaming;           /**< Is it streaming? */
        bool compressed;          /**< Is its output compressed? */
        bool shouldSnap;          /**< Should it save a snapshot of the next frame? */
    };


    // Objects
    SEQWriter *seqWriters[ N_CHANNELS ];
    FrameQueue frameQueues[ N_CHANNELS ];
	//SingleFrameBuffer frame_buffers[N_CHANNELS];
	SynchronizationQueue synchronizationQueues[N_CHANNELS];
	chrono::high_resolution_clock::time_point lastUIUpdate[N_CHANNELS];

	// Camera ROIs. Changes when values are set in UI
    int ROIs[ CameraController::Cameras::NUM_CAMERAS ][ ROI_SIZE ];
	// Same ROIs. Does not change when UI is updated (for resetting).
	int originalROIs[CameraController::Cameras::NUM_CAMERAS][ROI_SIZE];

    stream_attributes streamAttributes[ N_CHANNELS ];

    // Streamer control
    bool recording;
    bool running;

	std::string workingDir;
	
	// Camera Controller
    CameraController *camera;

	std::chrono::high_resolution_clock::time_point startTime;

    const std::string currentDateTime();

    void imageProcessor( Channels channel );
	void PGImageTransporter(FlyCapture2::Image* pImage, const void* pCallbackData);
	void SaveSnapshot(Streamer::Channels channel, QImage* rawImage, CameraFrame* current_frame);

    static void PGImageCleanup( void* );
    static void DSImageCleanup( void* );

    uchar* YUY2RGB( uchar* data, int width, int height );
};