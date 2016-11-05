/** 
 * @file streamer.cpp
 * @brief UI video streamer
 *
 * @author Santiago Navonne
 * TODO
 */ 

// Project includes
#include "streamer.h"
#include "seq_writer.h"
#include "exceptions.h"
#include <math.h>

#include <sstream>

Streamer *Streamer::transporterObject = NULL;
/**
 * @brief Streamer constructor
 * @arg None
 */
Streamer::Streamer( CameraController* _camera )
{
    // Default values
	maxDepthMM = MAX_DEPTH_DEFAULT;

    // Initialize stream attributes structs
    for ( int i = 0; i < N_CHANNELS; i++ )
    {
        streamAttributes[ i ].shouldSnap = false;
        streamAttributes[ i ].recording = false;
        streamAttributes[ i ].compressed = false;
        streamAttributes[ i ].streaming = false;
    }

    /* SEQWriters */
    seqWriters[ Channels::PointGreyFront ] = new SEQWriter( Channels::PointGreyFront );
    seqWriters[ Channels::PointGreyTop ] = new SEQWriter( Channels::PointGreyTop );
    seqWriters[ Channels::Color ] = new SEQWriter( Channels::Color );
    seqWriters[ Channels::Depth ] = new SEQWriter( Channels::Depth );
	seqWriters[Channels::IR] = new SEQWriter(Channels::IR);

	/* ROIs */
	ROIs[ CameraController::Cameras::PointGreyTop ][ ROICoordinates::X ] = 0;
	ROIs[ CameraController::Cameras::PointGreyTop ][ ROICoordinates::Y ] = 0;
	ROIs[ CameraController::Cameras::PointGreyTop ][ ROICoordinates::W ] = 1920;
	ROIs[ CameraController::Cameras::PointGreyTop ][ ROICoordinates::H ] = 1200;

	ROIs[ CameraController::Cameras::PointGreyFront ][ ROICoordinates::X ] = 0;
	ROIs[ CameraController::Cameras::PointGreyFront ][ ROICoordinates::Y ] = 0;
	ROIs[ CameraController::Cameras::PointGreyFront ][ ROICoordinates::W ] = 1920;
	ROIs[ CameraController::Cameras::PointGreyFront ][ ROICoordinates::H ] = 1200;

	ROIs[ CameraController::Cameras::Depth ][ ROICoordinates::X ] = 0;
	ROIs[ CameraController::Cameras::Depth ][ ROICoordinates::Y ] = 0;
	ROIs[ CameraController::Cameras::Depth ][ ROICoordinates::W ] = 320;
	ROIs[ CameraController::Cameras::Depth ][ ROICoordinates::H ] = 240;

	ROIs[ CameraController::Cameras::Color ][ ROICoordinates::X ] = 0;
	ROIs[ CameraController::Cameras::Color ][ ROICoordinates::Y ] = 0;
	ROIs[ CameraController::Cameras::Color ][ ROICoordinates::W ] = 640;
	ROIs[ CameraController::Cameras::Color ][ ROICoordinates::H ] = 480;


	// Set original_ROIs to the same values 
	for (int x = 0; x < CameraController::Cameras::NUM_CAMERAS; x++) {
		for (int y = 0; y < ROI_SIZE; y++) {
			originalROIs[x][y] = ROIs[x][y];
		}
	}

	isPGswitched = false;

	// Overall streaming indicator
	running = false;
	
	// Overall recording indicator
	record = false;
		
	// Objects from other classes to grab frames
    camera = _camera;

    // We really should only have one of these.
    transporterObject = this;

	startTime = chrono::high_resolution_clock::now();
}

/**
 * @brief Streamer destructor
 * @arg None
 */
Streamer::~Streamer( void )
{
    // Stop image transporters
	running = false;
	camera->getDepthSenseContext().quit();
	this_thread::sleep_for(std::chrono::milliseconds(1000)); // give threads time to react and shut down
}

/**
 * @brief Changes the Region of Interest for a given camera
 * @param camera The camera the ROI should be changed for
 * @param x X coordinate of ROI
 * @param y Y coordinate of ROI
 * @param w W dimension of ROI
 * @param h H dimension of ROI
 */
void Streamer::setROI( CameraController::Cameras camera, int x, int y, int w, int h )
{
    ROIs[ camera ][ ROICoordinates::X ] = x;
    ROIs[ camera ][ ROICoordinates::Y ] = y;
    ROIs[ camera ][ ROICoordinates::W ] = w;
    ROIs[ camera ][ ROICoordinates::H ] = h;
}

/**
 * @brief Changes the compressed attribute for a camera.
 * @param camera The camera the attribute should be changed for.
 * @param compressed The new value for the attribute.
 */
void Streamer::setCompressed( CameraController::Cameras camera, bool compressed )
{
    streamAttributes[ camera ].compressed = compressed;
}

/**
 * @brief Saves a snapshot from a camera stream.
 * @param camera The camera the attribute should be changed for.
 *
 * @note The stream must be streaming for this to take immediate effect.
 */
void Streamer::saveSnapshot( CameraController::Cameras camera )
{
    streamAttributes[ camera ].shouldSnap = true;
}

/**
 * @brief Change the working directory
 * @param workingDir The new working directory.
 * @returns void.
 */
void Streamer::setCurrentWorkingDir( string workingDir )
{
	this->workingDir = workingDir + "/";
}

/**
 * @brief Accessor for a ROI coordinate.
 * @param camera The camera whose ROI coordinate must be accessed.
 * @param value The coordinate type.
 * @returns The ROI value.
 */
int Streamer::getROI( CameraController::Cameras camera, ROICoordinates value )
{
    return ROIs[ camera ][ value ];
}

/**
* @brief Accessor for a ROI coordinate.
* @param camera The camera whose ROI coordinate must be accessed.
* @param value The coordinate type.
* @returns The ROI value.
*/
int Streamer::getOriginalROI(CameraController::Cameras camera, ROICoordinates value)
{
	return originalROIs[camera][value];
}

/**
 * @brief Returns the current date and time in the format "YYYYMMDD_HH-MM-SS"
 * @arg None.
 * @returns String with the current date and time.
 */
const string Streamer::currentDateTime() 
{
	time_t time = chrono::system_clock::to_time_t( chrono::system_clock::now() );
	stringstream ss; 
    ss << put_time( localtime( &time ), "%Y%m%d_%H-%M-%S" );
	return ss.str();
}

/**
 * @brief Push a CameraFrame to the front of the queue.
 * @param data The CameraFrame to be pushed.
 * @returns void.
 */
void FrameQueue::push( CameraFrame *data )
{
    this->mutex.lock();
    this->queue.push( data );
    this->mutex.unlock();
}

/**
* @brief Push a CameraFrame to the buffer, replacing previous frame if necessary.
* @param data The CameraFrame to be pushed.
* @returns void.

void SingleFrameBuffer::push(CameraFrame* frame) {
	mutex.lock();
	if (current_frame) {
		delete current_frame;
		qDebug() << "dropped a frame!" << endl;
	}
	current_frame = frame;
	mutex.unlock();
}
*/

void SynchronizationQueue::push(CameraFrame* frame) {
	mutex.lock();
	if (current_frame_queue.size() >= max_synchronization_queue_size) {
		auto temp = current_frame_queue.front();
		current_frame_queue.pop();
		delete temp;
#ifdef DEBUG
		qDebug() << "dropped a frame!" << endl;
#endif
	}
	current_frame_queue.push(frame);
	mutex.unlock();
}

/**
 * @brief Pop a CameraFrame from the back of the queue.
 * @arg None
 * @returns The popped CameraFrame.
 */
CameraFrame* FrameQueue::pop()
{
    CameraFrame *data;
    if ( this->queue.empty() )
        return NULL;
    this->mutex.lock();
    data = this->queue.front();
    this->queue.pop();
    this->mutex.unlock();
    return data;
}
/**
* @brief Repeatedly calls the imageTransporter function, passing in which cameras should be checked.
* @arg None
* @returns void
*/
void Streamer::imageTransporter() {
	while (running) {
		checkFrameBuffer(
			(streamAttributes[Channels::PointGreyTop].streaming || (streamAttributes[Channels::PointGreyTop].recording && recording)),
			(streamAttributes[Channels::PointGreyFront].streaming || (streamAttributes[Channels::PointGreyFront].recording && recording)),
			(streamAttributes[Channels::Depth].streaming || (streamAttributes[Channels::Depth].recording && recording)),
			(streamAttributes[Channels::Color].streaming || (streamAttributes[Channels::Color].recording && recording)));
		// Sleep a bit, so we're not using 100% CPU
		this_thread::sleep_for(std::chrono::milliseconds(1)); 
	}
}

/**
* @brief Check frame buffers. If all enabled cameras have a frame ready, queue all frames and reset buffers.
* @arg p_pgTop PG Top camera enabled
* @arg p_pgFront PG Front camera enabled
* @arg p_depth Depth camera enabled
* @arg p_color Color camera enabled
* @returns void
*
* This ensures synchronization between cameras, because (assuming all cameras have constant frame-rate),
* all frames will be recorded within 1/(slowest frame rate) seconds of each other.
* 
*/
void Streamer::checkFrameBuffer(bool p_pgTop, bool p_pgFront, bool p_depth, bool p_color)
{
	// Make a list of which channels are enabled
	vector<Channels> channels_to_check;
	if (p_pgTop) {
		channels_to_check.push_back(Channels::PointGreyTop);
	}
	if (p_pgFront) {
		channels_to_check.push_back(Channels::PointGreyFront);
	}
	if (p_depth) {
		channels_to_check.push_back(Channels::Depth);
	}
	if (p_color) {
		channels_to_check.push_back(Channels::Color);
	}

	// If no cameras enabled, do nothing
	if (channels_to_check.size() == 0) {
		return;
	}

	// Acquire mutexes. Should not deadlock AS LONG AS this is the only function that ever acquires 
	// more than one mutex (it currently is).
	for (auto& channel : channels_to_check) {
		synchronizationQueues[channel].mutex.lock();
	}

	// Check if all enabled channels have a frame ready
	bool return_value = true;
	for (auto& channel : channels_to_check) {
		//return_value = return_value && (frameBuffers[channel].current_frame != nullptr);
		return_value = return_value && (synchronizationQueues[channel].current_frame_queue.size() > 0);
	}


	if (return_value) { // All channels have a frame ready
		// Check if any of the queues for enabled channels are full
		bool queue_full = false;
		for (auto& channel : channels_to_check) {
			if (frameQueues[channel].queue.size() > MAX_QUEUE_SIZE) { 
				queue_full = true;
			}
		}

		// If all queues have space, push frames to queues and clear buffers
		if (!queue_full) {
			for (auto& channel : channels_to_check) {
				frameQueues[channel].push(synchronizationQueues[channel].current_frame_queue.front());
				synchronizationQueues[channel].current_frame_queue.pop();
			}
		}
		else {
			qDebug() << "Queue full, can't push!" << endl;
		}
	}

	// Release mutexes
	for (auto& channel : channels_to_check) {
		synchronizationQueues[channel].mutex.unlock();
	}
}

/**
* @brief Helper function to pass as a destructor function callback
* @arg data Pointer to data to be deleted
* @returns void
*/
static void cleanme(void* data) {
	delete data;
}

/**
* @brief Repeatedly checks if queues have frames ready, and if so displays them on UI and saves to disk.
* @arg channel Queue channel to monitor
* @returns void
*/
void Streamer::imageProcessor( Channels channel )
{
	int dataIndex;
    
    SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL );

    CameraController::Cameras cam;

    switch ( channel )
    {
    case Channels::PointGreyTop:
        cam = CameraController::Cameras::PointGreyTop;
        dataIndex = Channels::PointGreyTop - Channels::PointGreyTop;
        break;
    case Channels::PointGreyFront:
        cam = CameraController::Cameras::PointGreyFront;
        dataIndex = Channels::PointGreyFront - Channels::PointGreyTop;
        break;
    case Channels::Color:
        cam = CameraController::Cameras::Color;
        dataIndex = Channels::Color - Channels::Color;
        break;
    case Channels::Depth:
    default: // Intentional fall-through
        cam = CameraController::Cameras::Depth;
        dataIndex = Channels::Depth - Channels::Color;
        break;
    }

    while ( running && ( streamAttributes[ channel ].streaming || streamAttributes[ channel ].recording ) ) { 
		QImage rawImage;
		CameraFrame *currentFrame;
		QImage roiImage;
		QImage scaledImage;
        // Only consider the appropriate Region-of-Interest
		QRect roi = QRect( ROIs[ cam ][ ROICoordinates::X ], 
                           ROIs[ cam ][ ROICoordinates::Y ], 
                           ROIs[ cam ][ ROICoordinates::W ], 
                           ROIs[ cam ][ ROICoordinates::H ] );
			
        // Grab stuff from the queue
        while ( frameQueues[ channel ].queue.empty() )
        {
			this_thread::sleep_for(std::chrono::milliseconds(2));
            if ( !running || ( !streamAttributes[ channel ].streaming && !streamAttributes[ channel ].recording ) )
                return;
        }
        currentFrame = frameQueues[ channel ].pop();
		
        if ( !currentFrame )
            continue;

        // Assign the image data however necessary
        if ( currentFrame->PGData )
        {
			// We decide to make a copy of the frame data here, because otherwise we can't call the currentFrame destructor 
			// as it frees the frame memory. If we re-wrote the destructor to not free this memory we could make it work, but 
			// then other portions of the code would have to be modified to free their own data (ugly).
			
			uchar* data_to_pass = new uchar[currentFrame->PGData->GetDataSize()];
			memcpy(data_to_pass, currentFrame->PGData->GetData(), currentFrame->PGData->GetDataSize());

			rawImage = QImage::QImage(data_to_pass,
				currentFrame->PGData->GetCols(),
				currentFrame->PGData->GetRows(),
				QImage::Format::Format_Grayscale8,
				cleanme,
				data_to_pass);

			delete currentFrame;
        }
        else 
        {
            CameraController::FrameSize frameSize = CameraController::getDepthSenseFormatSize( currentFrame->imageFormat );
            if ( channel == Channels::Color ) // Color Camera
                rawImage = QImage::QImage( currentFrame->DSData8,
                                           frameSize.width,
                                           frameSize.height,
                                           QImage::Format::Format_RGB888,
                                           DSImageCleanup,
										   currentFrame).rgbSwapped(); // We need to swap Red and Blue Channels
            else // Depth Camera
            {
				// We need to copy the frame data because I can't figure out how to get a pointer to the original memory
				// that is accepted by the QImage constructor (it wants a uchar*, and we have a weird 16 bit pointer class type)
				auto size = currentFrame->DSData16.size();
				int16_t* my_data = new int16_t[size];
				memcpy(my_data, currentFrame->DSData16, size * sizeof(int16_t));

				rawImage = QImage::QImage((uchar*)my_data,
					frameSize.width,
					frameSize.height,
					QImage::Format::Format_RGB16, cleanme, my_data);

				// Choose color table based on grey checkbox
				if (true) { // For now, we'll just always convert to 8-bit

					scaledImage = rawImage.copy();
					scaledImage = scaledImage.convertToFormat(QImage::Format::Format_Grayscale8);

					// Do scaling. We want pixels to be 0 when >=480mm from camera, and 255 when the depth is <=225 from camera, and linear slope in between.

					float slope = -255 / (480 - 225);
					int y_intercept = 255 - slope * 225;

					for (int i = 0; i < scaledImage.byteCount(); i += 1) {

						// I'm not really sure how this works. The camera gives 16-bit depth data. Interpreting this as 2 8-bit channels
						// gives one channel that is a binary depth threshold map, and one channel that has repeating depth stripes.

						// However, converting this to 888 format gives one channel that looks like how I expect depth should look.
						// I'm not sure what function converts the 16-bit data to this, so I'm just going to use this as my 8-bit conversion.
						// The other channels in the 888 format contain either a threshold map or what appears to be noise.
						
						// For each pixel in the RGB16 representation, there are two bits. The second byte by index contains the upper 3 bits of the depth in the lower 3 bits. 
						// In an overflow case, this byte appears to be 1111101.
						// The first byte contains the lower 8 bits of the depth.

						unsigned int value;

						unsigned char a = rawImage.bits()[2 * i + 0];
						unsigned char b = rawImage.bits()[2 * i + 1];
						
						// Shift upper bits up 8, and add lower bits. Now, value contains 16-bit depth data.
						value = (((unsigned int)b) << 8)  + a;

						// Full-range display:
						// Scale this down to fit in [0, 255]. Since only 11 bits are actually used, use (255/2^11) for scaling constant.
						//rawImage.bits()[i] = (value * 255 / pow(2, 11));

						// Display scaling as requested by Weizhe
						
						int scaled_value = ((int)value) * slope + y_intercept;

						if (scaled_value > 255) {
							scaled_value = 255;
						}
						else if (scaled_value < 0) {
							scaled_value = 0;
						}

						scaledImage.bits()[i] = scaled_value;
					}
				}
            }
        }
		
        // Save a snapshot if necessary
		if (streamAttributes[channel].shouldSnap) {
			//SaveSnapshot(channel, &roiImage, currentFrame);
			string timeStamp = currentDateTime();

			auto file_name = QString::fromStdString(workingDir + "snapshots/"
				"Mouse_" +
				timeStamp +
				"_" +
				SEQWriter::fileNameChannels[channel] +
				"_" +
				".jpeg");

			auto directory = QString::fromStdString(workingDir + "snapshots/");
			if (!(CreateDirectoryW(SEQWriter::s2ws(directory.toStdString()).c_str(), NULL) ||
				ERROR_ALREADY_EXISTS == GetLastError()))
			{
				// Couldn't create directory!
			}

			int compressed_size = 0;
			unsigned char* _compressedImage = NULL;

			SEQWriter::compressJPEG(&rawImage, _compressedImage, compressed_size, rawImage.width(), rawImage.height()); // LibJPEG-turbo compression
			QFile myFile;
			myFile.setFileName(file_name);
			auto success = myFile.open(QIODevice::WriteOnly);
			auto seqFileStream = new QDataStream(&myFile);

			seqFileStream->writeRawData((char*)_compressedImage, compressed_size);
			tjFree(_compressedImage);
			myFile.close();
			delete seqFileStream;


			streamAttributes[channel].shouldSnap = false;

			if (channel == Depth) {
				// Do we have the IR frame here?
				if (currentFrame->DSConfidenceMap) {
					auto file_name = QString::fromStdString(workingDir + "snapshots/"
						"Mouse_" +
						timeStamp +
						"_" +
						SEQWriter::fileNameChannels[Channels::IR] +
						"_" +
						".jpeg");

					int compressed_size = 0;
					unsigned char* _compressedImage = NULL;

					CameraController::FrameSize frameSize = CameraController::getDepthSenseFormatSize(currentFrame->imageFormat);
					auto size = currentFrame->DSConfidenceMap.size();
					int16_t* my_data = new int16_t[size];
					memcpy(my_data, currentFrame->DSConfidenceMap, size * sizeof(int16_t));

					auto confidenceImage = QImage::QImage((uchar*)my_data,
						frameSize.width,
						frameSize.height,
						QImage::Format::Format_RGB16, cleanme, my_data);

					SEQWriter::compressJPEG(&confidenceImage, _compressedImage, compressed_size, rawImage.width(), rawImage.height()); // LibJPEG-turbo compression

					QFile myFile;


					myFile.setFileName(file_name);
					auto success = myFile.open(QIODevice::WriteOnly);
					auto seqFileStream = new QDataStream(&myFile);

					seqFileStream->writeRawData((char*)_compressedImage, compressed_size);
					tjFree(_compressedImage);
					myFile.close();
					delete seqFileStream;
				}
			}
		}

        // And process the ROI for recording
	    
		if ( streamAttributes[ channel ].recording && recording ) 
        {
			
			// If channel is Depth, also write the "Confidence" data
			if (channel == Channels::Depth) {
				// Do we have a IR frame?
				if (currentFrame->DSConfidenceMap) {
					CameraController::FrameSize frameSize = CameraController::getDepthSenseFormatSize(currentFrame->imageFormat);
					auto size = currentFrame->DSConfidenceMap.size();
					int16_t* my_data = new int16_t[size];
					memcpy(my_data, currentFrame->DSConfidenceMap, size * sizeof(int16_t));

					auto confidenceImage = QImage::QImage((uchar*)my_data,
						frameSize.width,
						frameSize.height,
						QImage::Format::Format_RGB16, cleanme, my_data);

#ifdef COMPATIBILITY_MODE

					// Downscale to 8-bit
					auto scaled_IR = confidenceImage.copy();
					scaled_IR = scaledImage.convertToFormat(QImage::Format::Format_Grayscale8);

					// Apply the downsampling algorithm. TODO: fix having two copies of this same code.
					for (int i = 0; i < scaled_IR.byteCount(); i += 1) {

						unsigned int value;

						float slope = -255 / (480 - 225);
						int y_intercept = 255 - slope * 225;

						unsigned char a = confidenceImage.bits()[2 * i + 0];
						unsigned char b = confidenceImage.bits()[2 * i + 1];

						value = (((unsigned int)b) << 8) + a;

						int scaled_value = ((int)value) * slope + y_intercept;

						if (scaled_value > 255) {
							scaled_value = 255;
						}
						else if (scaled_value < 0) {
							scaled_value = 0;
						}

						scaled_IR.bits()[i] = scaled_value;
					}

					seqWriters[Channels::IR]->writeFrame(&scaled_IR.copy(roi),
						currentFrame->timestampSeconds,
						currentFrame->timestampMilliSeconds);

#else
					// Just save 16 bit data
					seqWriters[Channels::IR]->writeFrame(&confidenceImage.copy(roi),
						currentFrame->timestampSeconds,
						currentFrame->timestampMilliSeconds);
#endif
				}
				// Also handle the regular depth frame
				
				// If compatibility mode, save scaled image. Else, save 16-bit image
#ifdef COMPATIBILITY_MODE
				roiImage = scaledImage.copy(roi);
				seqWriters[channel]->writeFrame(&roiImage,
					currentFrame->timestampSeconds,
					currentFrame->timestampMilliSeconds);
#else
				roiImage = rawImage.copy(roi);
				seqWriters[channel]->writeFrame(&roiImage,
					currentFrame->timestampSeconds,
					currentFrame->timestampMilliSeconds);
#endif
				
			}
			else {
				roiImage = rawImage.copy(roi);
				seqWriters[channel]->writeFrame(&roiImage,
					currentFrame->timestampSeconds,
					currentFrame->timestampMilliSeconds);
			}
        }
		
        // Display the image - must do this at the end, since memory is freed after the image is displayed
        if ( ( channel != Channels::IR ) && streamAttributes[ channel ].streaming )
        {
			// If the UI thread is still working on displaying the previous frame
			// when we emit this frame, this frame may get dropped. If so, we get memory leaks.
			// We fixed this problem by only updating the UI after a certain period of time has 
			// elapsed since the previous update. If this is longer than the normal frame interval,
			// the UI will be updated at a lower frame-rate. 
            
			chrono::high_resolution_clock::time_point now = chrono::high_resolution_clock::now();
			int time_diff = chrono::duration_cast<chrono::milliseconds>(now - lastUIUpdate[channel]).count() % 1000000000;
			
			if (time_diff > UI_UPDATE_RATE) {
				lastUIUpdate[channel] = now;
				QImage cropped_image;
				if (channel == Channels::Depth) {
					cropped_image = scaledImage.copy(roi);
				}
				else {
					cropped_image = rawImage.copy(roi);
				}
				
				emit updateCamera(cam, cropped_image);
			}
        }
	}
}


/**
 * @brief Transport Point Grey Camera frames from the CameraController to the Queue. 
 * @arg None.
 * @returns void.
 *
 * The function queries the CameraController for new images from all Point Grey
 * sensors, slaps a timestamp on them, and puts them on the appropriate queue for
 * consumption by the corresponding imageProcessor thread.
 * The ImageTransporter() threads are synchronized through a Checkpoint object.
 * 
 * This was originally a loop running on its own thread and polls for new images. It
 * has been modified to be a simple image transporter.
 * pCallbackData is a pointer to some data that is included when the event handler is attached.
 */
void Streamer::PGImageTransporter(FlyCapture2::Image* pImage, const void* pCallbackData)
{
	if (running) {
		// Compute timestamp
		chrono::high_resolution_clock::time_point now = chrono::high_resolution_clock::now();
		int timestampOrig = chrono::duration_cast<chrono::milliseconds>(now - startTime).count() % 1000000000;
		int sec = (int)(timestampOrig / 1000);
		short ms = (short)(timestampOrig % 1000);

	#ifdef DEBUG
		//qDebug() << "PG: " << (int)pCallbackData << " " << sec << ms << endl;
		//qDebug() << "Color buffer size: " << synchronizationQueues[Channels::Color].current_frame_queue.size() << endl;
		//qDebug() << data.timeOfCapture << endl;
	#endif // DEBUG

		if (*((CameraController::Cameras*) pCallbackData) == CameraController::Cameras::PointGreyFront
			&& (streamAttributes[Channels::PointGreyFront].recording || streamAttributes[Channels::PointGreyFront].streaming)) {
			FlyCapture2::Image* image_copy = new FlyCapture2::Image();
			image_copy->DeepCopy(pImage);
			synchronizationQueues[Channels::PointGreyFront].push(new CameraFrame(image_copy, sec, ms));
		}
		else if (*((CameraController::Cameras*) pCallbackData) == CameraController::Cameras::PointGreyTop &&
			(streamAttributes[Channels::PointGreyTop].recording || streamAttributes[Channels::PointGreyTop].streaming)) {
			// We make a copy of the image because the original seems to be freed after this function returns
			FlyCapture2::Image* image_copy = new FlyCapture2::Image();
			image_copy->DeepCopy(pImage);
			synchronizationQueues[Channels::PointGreyTop].push(new CameraFrame(image_copy, sec, ms));
		}
	}
}

void Streamer::depthSenseColorTransporter( DepthSense::ColorNode::NewSampleReceivedData data )
{
    if ( !streamAttributes[ Channels::Color ].streaming && !streamAttributes[ Channels::Color ].recording)
        return;
	if (!running) { return; }
    CameraFrame *theFrame;

    uint sec = data.timeOfCapture / 1000000;
    uint ms = data.timeOfCapture / 1000 - sec * 1000;

#ifdef DEBUG
	qDebug() << "Color: " << sec << ms << endl;
	qDebug() << "Color buffer size: " << synchronizationQueues[Channels::Color].current_frame_queue.size() << endl;
	//qDebug() << data.timeOfCapture << endl;
#endif // DEBUG

    theFrame = new CameraFrame( data.colorMap,
                                data.captureConfiguration.frameFormat,
                                sec,
                                ms );
	synchronizationQueues[Channels::Color].push(theFrame);
}

void Streamer::depthSenseDepthTransporter( DepthSense::DepthNode::NewSampleReceivedData data )
{
    if ( !streamAttributes[ Channels::Depth ].streaming && !streamAttributes[ Channels::Depth ].recording )
        return;
	if (!running) { return; }
    CameraFrame *theFrame;

    uint sec = data.timeOfCapture / 1000000;
    uint ms = data.timeOfCapture / 1000 - sec * 1000;

#ifdef DEBUG
	qDebug() << "Depth: " << sec << ms << endl;
	//qDebug() << data.timeOfCapture << endl;
	qDebug() << "Depth buffer size: " << synchronizationQueues[Channels::Depth].current_frame_queue.size() << endl;
#endif // DEBUG

	
	
    theFrame = new CameraFrame( data.depthMap,
								data.confidenceMap,
                                data.captureConfiguration.frameFormat,
                                sec,
                                ms );
	synchronizationQueues[Channels::Depth].push(theFrame);
}

/**
 * @brief Run the whole streaming pipeline.
 * @arg None.
 * @returns void.
 *
 * The function manages the transporter and processor threads that acquire
 * new images from all the enabled sensors, display them on the GUI, and
 * save them to files as required (compressing if necessary).
 */
void Streamer::run()
{
	running = true;
	int num_pg_initialized = 0;
    // Process each frame in parallel. Multithreading!
    try
    {
		num_pg_initialized = camera->initPG();
    }
    catch ( exception_t e )
    {
        if ( e == EXCEPTION_PG );
            // TODO report error
    }

    try
    {
        camera->initIntel( &Streamer::depthSenseDepthWrapper, &Streamer::depthSenseColorWrapper );
    }
    catch ( exception_t e )
    {
        if ( e == EXCEPTION_INTEL );
            // TODO report error
    }

	// This thread will generate callbacks for the depthsense camera
    std::thread( &DepthSense::Context::run, camera->getDepthSenseContext() ).detach();

	// These threads will generate callbacks for the pointgrey camera
	// We pass the index of the camera as the pCallbackInfo - will need parsed to the actual camera on the other side
	for (int i = 0; i < num_pg_initialized; ++i) {
		CameraController::Cameras* this_camera = new CameraController::Cameras;
		*this_camera = (CameraController::Cameras)i;
		camera->CameraProps[i].Camera->StartCapture(PGWrapper, this_camera);
		//std::thread(&FlyCapture2::Camera::StartCapture, camera->CameraProps[i].Camera, PGWrapper, this_camera).detach();
	}

	// This thread runs the synchronization loop
	std::thread(&Streamer::imageTransporter, this).detach();
}

/**
 * @brief Start streaming a data from a camera
 * @param camera The camera from which data should be streamed.
 * @returns void.
 */
void Streamer::startStreaming( CameraController::Cameras camera )
{
	// Display: each channel processed separately
    Channels channel;
    switch ( camera )
    {
    case CameraController::Cameras::PointGreyFront:
        channel = Channels::PointGreyFront;
        break;
    case CameraController::Cameras::PointGreyTop:
        channel = Channels::PointGreyTop;
        break;
    case CameraController::Cameras::Color:
        channel = Channels::Color;
        break;
    default:
        channel = Channels::Depth;
        break;
    }

    streamAttributes[ channel ].streaming = true;
    // Start the image processor thread pretty much only if it hadn't already been started
	if (!streamAttributes[channel].recording) {
        std::thread( &Streamer::imageProcessor, this, channel ).detach();
	}
}

/**
 * @brief Stop streaming data from a camera
 * @param camera The camera from which data should be stopped
 * @returns void.
 */
void Streamer::stopStreaming( CameraController::Cameras camera )
{
    // Record and Display: each channel processed separately
    Channels channel;
    switch ( camera )
    {
    case CameraController::Cameras::PointGreyFront:
        channel = Channels::PointGreyFront;
        break;
    case CameraController::Cameras::PointGreyTop:
        channel = Channels::PointGreyTop;
        break;
    case CameraController::Cameras::Color:
        channel = Channels::Color;
        break;
    default:
        channel = Channels::Depth;
        break;
    }

    streamAttributes[ channel ].streaming = false;
}

/**
 * @brief Start recording all selected videos.
 * @param pgt Whether the Point Grey Top camera stream should be recorded.
 * @param pgf Whether the Point Grey Front camera stream should be recorded.
 * @param color Whether the Color camera stream should be recorded.
 * @param depth Whether the Depth camera stream should be recorded.
 * @returns void.
 */
void Streamer::startRecording( bool pgt, bool pgf, bool color, bool depth )
{
	string dateTime = currentDateTime();
		
	// Open PointGreyTop file stream and start thread
	if ( pgt )
    {
        seqWriters[ Channels::PointGreyTop ]->startRecording( workingDir, 
                                                              ROIs[ Channels::PointGreyTop ][ ROICoordinates::W ],
                                                              ROIs[ Channels::PointGreyTop ][ ROICoordinates::H ],
                                                              streamAttributes[ Channels::PointGreyTop ].compressed,
                                                              dateTime, isPGswitched );
        streamAttributes[ Channels::PointGreyTop ].recording = true;
        if ( !streamAttributes[ Channels::PointGreyTop ].streaming )
            std::thread ( &Streamer::imageProcessor, this, Channels::PointGreyTop ).detach();
    }

	// Open PointGreyFront file stream and start thread
	if ( pgf )
    {
        seqWriters[ Channels::PointGreyFront ]->startRecording( workingDir, 
                                                              ROIs[ Channels::PointGreyFront ][ ROICoordinates::W ],
                                                              ROIs[ Channels::PointGreyFront ][ ROICoordinates::H ],
                                                              streamAttributes[ Channels::PointGreyFront ].compressed,
															  dateTime, isPGswitched);
        streamAttributes[ Channels::PointGreyFront ].recording = true;
        if ( !streamAttributes[ Channels::PointGreyFront ].streaming )
            std::thread ( &Streamer::imageProcessor, this, Channels::PointGreyFront ).detach();
    }

	// Open Color file stream and start thread
	if ( color )
    {
        seqWriters[ Channels::Color ]->startRecording( workingDir, 
                                                       ROIs[ Channels::Color ][ ROICoordinates::W ],
                                                       ROIs[ Channels::Color ][ ROICoordinates::H ],
                                                       streamAttributes[ Channels::Color ].compressed,
													   dateTime, isPGswitched);
        streamAttributes[ Channels::Color ].recording = true;
        if ( !streamAttributes[ Channels::Color ].streaming )
            std::thread ( &Streamer::imageProcessor, this, Channels::Color ).detach();
    }

	// Open Depth file stream and start thread
	if ( depth )
    {
        seqWriters[ Channels::Depth ]->startRecording( workingDir, 
                                                       ROIs[ Channels::Depth ][ ROICoordinates::W ],
                                                       ROIs[ Channels::Depth ][ ROICoordinates::H ],
                                                       streamAttributes[ Channels::Depth ].compressed,
													   dateTime, isPGswitched);
		seqWriters[Channels::IR]->startRecording(workingDir,
														ROIs[Channels::Depth][ROICoordinates::W],
														ROIs[Channels::Depth][ROICoordinates::H],
														streamAttributes[Channels::Depth].compressed,
														dateTime, isPGswitched);
        streamAttributes[ Channels::Depth ].recording = true;
        if ( !streamAttributes[ Channels::Depth ].streaming )
            std::thread ( &Streamer::imageProcessor, this, Channels::Depth ).detach();
	}

    recording = true; // Do this last so everybody starts at the same time.
}

/**
 * @brief Stop recording all active channels.
 * @arg None.
 * @returns void.
 */
void Streamer::stopRecording()
{
	recording = false;

	for (int c = Channels::IR; c >= 0 ; c--)
    {
		// IR channel stops with depth, because it doesn't have its own check boxes
		if (c == Channels::IR) {
			if (streamAttributes[Channels::Depth].recording) {
				seqWriters[Channels::IR]->stopRecording();
			}
		}
		// Else, handle normally
		if (streamAttributes[c].recording) {
			streamAttributes[c].recording = false;
			seqWriters[c]->stopRecording();
		}
    }
}

/**
 * @brief Check if the player is stopped
 * @arg None.
 * @returns Whether the player is stopped.
 */
bool Streamer::isStopped()
{
	return !( this->running );
}

/**
 * @brief Clean up after a Point Grey image.
 * @param image Pointer to the image to be deleted.
 * @returns void.
 * @note PG image memory is always freed by this function.
 */
void Streamer::PGImageCleanup( void* image )
{
    delete ( (FlyCapture2::Image*) image );
}

/**
 * @brief Clean up after a DepthSense image
 * @param data Pointer to the previously acquired frame storage.
 * @returns void.
 * @note DS image memory is always freed by this function.
 */
void Streamer::DSImageCleanup( void* data )
{
    delete ( CameraFrame* )data;
}

/**
 * @brief YUY2 to RGB888 conversion.
 * @param data Pointer to YUY2 data.
 * @param width Width of the image.
 * @param height Height of the image.
 * @returns Pointer to RGB data.
 * @note The returned pointer is newly allocated memory that must be freed.
 */
uchar* Streamer::YUY2RGB( uchar* data, int width, int height )
{
    uchar bY0, bU, bY1, bV;
    uchar bR0, bG0, bB0, bR1, bG1, bB1;

    float fR0, fG0, fB0, fR1, fG1, fB1;
    int yuvMacroPixel;
    int rgbMacroPixel;

    uchar *rgbData = new uchar[ width * height * 3 ];
    for ( int row = 0; row < height; row++ )
    {
        for ( int macroCol = 0; macroCol < ( width >> 1 ); macroCol++ )
        {
            // Get location in memory
            yuvMacroPixel = ( row * ( width << 1 ) ) + ( macroCol * 4 );

            // Get YUV values for a macropixel = two pixels
            bY0 = data[ yuvMacroPixel + 0 ];
            bU  = data[ yuvMacroPixel + 1 ];
            bY1 = data[ yuvMacroPixel + 2 ];
            bV  = data[ yuvMacroPixel + 3 ];

            // Convert YUV to RGB for two pixels
            fR0 = bY0 + ( 1.370705 * ( bV - 128 ) );
            fG0 = bY0 - ( 0.698001 * ( bV - 128 ) ) - ( 0.337633 * ( bU - 128 ) );
            fB0 = bY0 + ( 1.732446 * ( bU - 128 ) );

            fR1 = bY1 + ( 1.370705 * ( bV - 128 ) );
            fG1 = bY1 - ( 0.698001 * ( bV - 128 ) ) - ( 0.337633 * ( bU - 128 ) );
            fB1 = bY1 + ( 1.732446 * ( bU - 128 ) );

            CLAMP( fR0, bR0 );
            CLAMP( fG0, bG0 );
            CLAMP( fB0, bB0 );
            CLAMP( fR1, bR1 );
            CLAMP( fG1, bG1 );
            CLAMP( fB1, bB1 );

            // Save RGB values for two pixels
            rgbMacroPixel = ( row * width * 3 ) + ( macroCol * 6 );
            rgbData[ rgbMacroPixel + 0 ] = bR0;
            rgbData[ rgbMacroPixel + 1 ] = bG0;
            rgbData[ rgbMacroPixel + 2 ] = bB0;
            rgbData[ rgbMacroPixel + 3 ] = bR1;
            rgbData[ rgbMacroPixel + 4 ] = bG1;
            rgbData[ rgbMacroPixel + 5 ] = bB1;
        }
    }

    return rgbData;
}