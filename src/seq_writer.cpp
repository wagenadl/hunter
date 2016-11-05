/** 
 * @file seq_writer.cpp
 * @brief SEQ file writer
 *
 * @author Santiago Navonne
 */ 

// Project includes
#include "seq_writer.h"

// Libraries
#include <QTGui/QImage.h>
#include <QTCore/QBuffer.h>


// C++
#include <chrono>

using namespace std;

//// Constants
const std::string SEQWriter::fileNameHead = "Mouse_";
const std::string SEQWriter::fileNameFoot = ".seq";
const std::string SEQWriter::fileNameCompressed = "J85";
const std::string SEQWriter::fileNameRaw = "Raw";
const std::string SEQWriter::compressionExt = "jpg";

// Set the proper number of bits per pixel. If compatiblity mode, use 8-bit depth and IR
#ifdef COMPATIBILITY_MODE
const int SEQWriter::bitsPerPixel[] = { 8, 8, 24, 8, 8 };
#else
const int SEQWriter::bitsPerPixel[] = { 8, 8, 24, 16, 16 };
#endif

const std::string SEQWriter::fileNameChannels[] = { "Top", "Front", "Color", "DepGr", "IR" };
const unsigned short SEQWriter::norpixString[] = { 'N', 'o', 'r', 'p', 'i', 'x', ' ', 's', 'e', 'q' };
const char SEQWriter::norpixDesc[] = "";

/**
 * @brief SEQWriter constructor
 * @arg None
 */
SEQWriter::SEQWriter( Streamer::Channels channel )
    : streamChannel( channel )
{
}

/**
 * @brief SEQWriter destructor
 * @arg None
 */
SEQWriter::~SEQWriter( void )
{
	
}

// Utility function for converting to a windows string
std::wstring SEQWriter::s2ws(const std::string& s)
{
	int len;
	int slength = (int)s.length() + 1;
	len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
	wchar_t* buf = new wchar_t[len];
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
	std::wstring r(buf);
	delete[] buf;
	return r;
}

/**
 * @brief Start recording to a SEQ file
 * @param workingDir The working directory where the file should be saved.
 * @param width The width of the stream being saved in pixels.
 * @param height The height of the stream being saved in pixels.
 * @param compressed Whether the channel is being compressed.
 * @param dateTime The date and time for the file's timestamp.
 * @returns void.
 */
void SEQWriter::startRecording( std::string workingDir, 
                                int width,
                                int height,
                                bool compressed, 
                                std::string dateTime,
								bool isPGswitched)
{
	Streamer::Channels current_chan = this->streamChannel;
	if (this->streamChannel == Streamer::Channels::PointGreyFront && isPGswitched) {
		current_chan = Streamer::Channels::PointGreyTop;
	}
	else if (this->streamChannel == Streamer::Channels::PointGreyTop && isPGswitched) {
		current_chan = Streamer::Channels::PointGreyFront;
	}

	QString path = QString::fromStdString(workingDir + "recordings/" +
													fileNameHead +
													dateTime +
													fileNameSeparator +
													fileNameChannels[current_chan] +
													fileNameSeparator +
													(compressed ? fileNameCompressed : fileNameRaw) +
													fileNameFoot);
	
	// Attempt to create the directory, if it doesn't already exist
	auto directory = QString::fromStdString(workingDir + "recordings/");
	if (!(CreateDirectoryW(s2ws(directory.toStdString()).c_str(), NULL) ||
		ERROR_ALREADY_EXISTS == GetLastError()))
	{
		// Couldn't create directory!
	}
    seqFile.setFileName(path);
	auto success = seqFile.open( QIODevice::WriteOnly );
    seqFileStream = new QDataStream( &seqFile );
	makeEmptyHeader();
	this->totalFrames = 0;
    this->compressed = compressed;
    this->width = width;
    this->height = height;
}



/**
 * @brief Stop recording to the SEQ file
 * @arg None.
 * @returns void.
 */
void SEQWriter::stopRecording()
{
    int bpp = bitsPerPixel[ streamChannel ];
    // Write the header
    writeHeader( width, height, bpp );

    // And close the file
	seqFile.close();
}

/**
* @brief Writes a frame to disk
* @param image Image to be written
* @param secs Timestamp, seconds portion
* @param ms Timestamp, milliseconds portion
* @returns void.
*/
void SEQWriter::writeFrame( QImage *image, int secs, short ms) 
{
    int initialPos = seqFile.pos();
    int finalPos;
	int32_t image_size = 0;
	unsigned char* _compressedImage = NULL;

	// Figure out how big the image is
	if (compressed) {
		compressJPEG(image, _compressedImage, image_size, width, height); // LibJPEG-turbo compression
	}
	else {
		image_size = image->width() * image->height() * bitsPerPixel[streamChannel] / 8;
	}

	// Write the image size
    // According to spec, the frame size is written before the image data ONLY for compressed images.
	// The old code wrote it before both.
#ifdef COMPATIBILITY_MODE
	seqFileStream->writeRawData((char*)&image_size, sizeof(int32_t));
#else
	if (compressed)
	{
		seqFileStream->writeRawData((char*)&image_size, sizeof(int32_t));
	}
#endif

    // Write data
    if ( compressed )
    {
		seqFileStream->writeRawData((char*)_compressedImage, image_size);
		tjFree(_compressedImage);
    }
    else
    {
		seqFileStream->writeRawData((char*)image->bits(), image_size);
    }

    // Write timestamp written after image bytes
	short mc = 0;
	seqFileStream->writeRawData( (char*) &secs, sizeof(int32_t) );
	seqFileStream->writeRawData( (char*) &ms, sizeof(int16_t) );
	seqFileStream->writeRawData( (char*) &mc, sizeof(int16_t) );

	// There should be no padding after the frame (I think.)
	// The old version had 8 bytes of padding
#ifdef COMPATIBILITY_MODE
	for (int i = 0; i < 8; i++)
		seqFileStream->writeRawData((char*)&null, sizeof(int8_t));
#endif

    // Keep track of how many frames were saved
    totalFrames++;
}

/**
* @brief Compresses a QImage using the LibJPEG-turbo compression library. 
* @param image Image to be compressed
* @param _compressedImage The location where the compressed image will be put
* @param compressed_size The size of the resulting image
* @returns void.
*/
void SEQWriter::compressJPEG(QImage* image, unsigned char*& _compressedImage, int& compressed_size, int width, int height)
{
	TJPF pixel_format;
	QImage frame_converted;
	int COLOR_COMPONENTS = 0; // How many channels are in the image?
	
	// Create the compressor
	tjhandle _jpegCompressor = tjInitCompress();

	// Figure out what image type we have
	if (image->format() == QImage::Format_Grayscale8) {
		pixel_format = TJPF::TJPF_GRAY;
		COLOR_COMPONENTS = 1;
	}
	else if (image->format() == QImage::Format_RGB888) {
		pixel_format = TJPF::TJPF_RGB;
		COLOR_COMPONENTS = 3;
	}
	else if (image->format() == QImage::Format_RGB16) {
		// JPEG doesn't support 16-bit RGB. We need to up-convert.
		frame_converted = image->convertToFormat(QImage::Format_RGB888);
		pixel_format = TJPF::TJPF_RGB;
		COLOR_COMPONENTS = 3;
	}
	else {
		throw std::invalid_argument("Image format not implemented!");
	}
	
	unsigned long _jpegSize = 0;
	
	int result;

	// We need to treat the different formats separately during compression
	if (image->format() == QImage::Format_RGB16) {
		 result = tjCompress2(_jpegCompressor, frame_converted.bits(), width, 0, height, pixel_format,
			&_compressedImage, &_jpegSize, TJSAMP_444, JPEG_QUALITY,
			TJFLAG_FASTDCT);
	}
	else if (image->format() == QImage::Format_Grayscale8) {
		result = tjCompress2(_jpegCompressor, image->bits(), width, 0, height, pixel_format,
			&_compressedImage, &_jpegSize, TJSAMP_GRAY, JPEG_QUALITY,
			TJFLAG_FASTDCT);
	}
	else {
		result = tjCompress2(_jpegCompressor, image->bits(), width, 0, height, pixel_format,
			&_compressedImage, &_jpegSize, TJSAMP_444, JPEG_QUALITY,
			TJFLAG_FASTDCT);
	}

	if (result != 0) {
		throw std::runtime_error("JPEG compression failed!");
	}
	tjDestroy(_jpegCompressor);

	compressed_size = _jpegSize;
}



/**
 * @brief Allocate space for a SEQ file header
 * @arg None.
 * @returns void.
 */
void SEQWriter::makeEmptyHeader( )
{
	for ( int i = 0; i < SEQ_HEADER_SIZE; i++ )
	{
		seqFileStream->writeRawData( &null, sizeof( char ) );
	}
}

/**
 * @brief Fill in the header for an open SEQ file
 * @param width Width of the images in the stream
 * @param height Height of the images in the stream
 * @param bpp_num Number of bits per pixel
 * @returns void.
 */
void SEQWriter::writeHeader( int width, int height, int bpp_num )
{

	// this was wrong on the old version.
#ifdef COMPATIBILITY_MODE
	int32_t bytesPerFrame = (width * height * bpp_num) / 1;
#else
	int32_t bytesPerFrame = (width * height * bpp_num) / 8;
#endif

	// this was wrong on the old version.
#ifdef COMPATIBILITY_MODE
	//int32_t trueImageSize = bytesPerFrame + sizeof(uint8_t);
	int32_t trueImageSize = bytesPerFrame + 8 * sizeof(uint8_t);
#else
	int32_t trueImageSize = bytesPerFrame + 8 * sizeof(uint8_t);
#endif

    int remainingHeader = SEQ_HEADER_SIZE; // Keep track of how much room is left in header

    //seek to beginning of file
	seqFile.seek( 0 );

    // Write header
    // Random magic number (unsigned int = 4 bytes)
	seqFileStream->writeRawData( (char*) &norpixVar, sizeof(uint32_t) );
    remainingHeader -= sizeof(uint32_t);
    // Norpix string
	for ( int i = 0; i < NORPIX_STRING_LENGTH; i++ )
        seqFileStream->writeRawData( (char*) &norpixString[ i ], sizeof(uint16_t) );
    remainingHeader -= NORPIX_STRING_LENGTH * sizeof(uint16_t);

    // Blank space (4 bytes)
	for (int i = 0; i < 4; i++) 
        seqFileStream->writeRawData( (char*) &null, sizeof(uint8_t) );
    remainingHeader -= 4;
	// Version number (int = 4 bytes)
	seqFileStream->writeRawData( (char*) &norpixVer, sizeof(int32_t) );
    remainingHeader -= sizeof(int32_t);
    // Header size (int = 4 bytes)
	seqFileStream->writeRawData( (char*) &norpixHeaderSize, sizeof(int32_t) );
    remainingHeader -= sizeof(int32_t);
	// Description (512 bytes)
	seqFileStream->writeRawData( (char*) &norpixDesc, NORPIX_DESC_LENGTH );
	for ( int i = 0; i < NORPIX_DESC_SIZE - NORPIX_DESC_LENGTH; i++ )
		seqFileStream->writeRawData( (char*) &null, sizeof(uint8_t) );
    remainingHeader -= NORPIX_DESC_SIZE;

	// Write CImage data
	seqFileStream->writeRawData( (char*) &width, sizeof(int32_t) );
	seqFileStream->writeRawData( (char*) &height, sizeof(int32_t) );
	seqFileStream->writeRawData( (char*) &bpp_num, sizeof(int32_t) );
	seqFileStream->writeRawData( (char*) &norpixBPS, sizeof(int32_t) );
	seqFileStream->writeRawData( (char*) &bytesPerFrame, sizeof(int32_t) );
    // Including image format, which depends on a bunch of things
    int imageFormat;
    if ( !compressed )
    {
        if ( streamChannel == Streamer::Channels::Color )
            imageFormat = SEQ_UNCOMPRESSED_COLOR;
        else
            imageFormat = SEQ_UNCOMPRESSED_GRAYSCALE;
    }
    else
    {
        if ( streamChannel == Streamer::Channels::Color )
            imageFormat = SEQ_JPEG_COLOR;
        else
            imageFormat = SEQ_JPEG_GRAYSCALE;
    }
	seqFileStream->writeRawData( (char*) &imageFormat, sizeof(int32_t) );
    remainingHeader -= 6 * sizeof(int32_t);

	// Write number of frames (int = 4 bytes)
	seqFileStream->writeRawData( (char*) &totalFrames, sizeof(int32_t) );
    remainingHeader -= sizeof(int32_t);
	// Write origin (int = 4 bytes)
	seqFileStream->writeRawData( (char*) &norpixOrigin, sizeof(int32_t) );
    remainingHeader -= sizeof(int32_t);
	// Write true image size (unsigned int = 4 bytes)
	seqFileStream->writeRawData( (char*) &trueImageSize, sizeof(uint32_t) );
    remainingHeader -= sizeof( uint32_t);

	// Write 8 bytes: framerate (double)
	double fps = 30.0; // TODO: This shouldn't be a constant
	seqFileStream->writeRawData( (char*) &fps, sizeof( double ) );
    remainingHeader -= sizeof( double );

	// Fill the rest of the header with nulls
	for (int i = 0; i < remainingHeader; i++)
		seqFileStream->writeRawData( (char*) &null, sizeof(uint8_t) );
}

/**
 * @brief Converts a hexadecimal character to an integer.
 * @param ch The character to be converted.
 * @returns The converted value.
 */
int32_t SEQWriter::hexCharToDecimal( char ch )
{
	ch = toupper(ch); // Work with uppercase for simplicity

	if ( ch >= 'A' && ch <= 'F' ) // 10+
		return 10 + ch - 'A';
	else // 0-10
		return ch - '0';
}

/**
 * @brief Converts a hexadecimal string to an integer.
 * @param ch The string to be converted.
 * @returns The converted value.
 */
int SEQWriter::hexToDec( const string &hex )
{
	int decimalValue = 0;
	for ( int i = 0; i < hex.size(); i++ )
		decimalValue = decimalValue * 16 + hexCharToDecimal( hex[ i ] );
	return decimalValue;
}
