/** 
 * @file seq_writer.h
 * @brief SEQ file writer
 *
 * @author Santiago Navonne
 * TODO
 */

#pragma once

// Project includes
#include "streamer.h"

// C++
#include <fstream>
#include <string>
#include <vector>

// Libraries
#include <QTCore/QFile.h>
#include <turbojpeg.h>

class Streamer;

class SEQWriter
{
public:
    SEQWriter( Streamer::Channels channel );
    ~SEQWriter( void );

    void startRecording( std::string workingDir, int width, int height, bool compressed, std::string dateTime, bool isPGswitched);
    void stopRecording();
    void writeFrame( QImage *image, int secs, short ms);
	static void compressJPEG(QImage* image, unsigned char*& _compressedImage, int& compressed_size, int width, int heigth);
	static std::wstring s2ws(const std::string& s);

	static const std::string fileNameChannels[Streamer::N_CHANNELS];

private:
    // Constants
    enum
    { 
        SEQ_HEADER_SIZE = 1024,            /**< Size of SEQ header in bytes. */
        SEQ_VER = 3,                       /**< SEQ file version. */
        NORPIX_STRING_LENGTH = 10,         /**< Length of the Norpix string. */
        NORPIX_DESC_LENGTH = 1,            /**< Length of the file description. */
        NORPIX_DESC_SIZE = 512,            /**< Size the description should be. */
        JPEG_QUALITY = 80,                 /**< JPEG quality specified in the header. */
        SEQ_UNCOMPRESSED_COLOR = 200,      /**< Identifier for uncompressed color images. */
        SEQ_JPEG_COLOR = 201,              /**< Identifier for JPEG color images. */
        SEQ_UNCOMPRESSED_GRAYSCALE = 100,  /**< Identifier for uncompressed grayscale images. */
        SEQ_JPEG_GRAYSCALE = 102,          /**< Identifier for JPEG grayscale images. */
    };
    static const char null = NULL;
    static const std::string fileNameHead;
    static const std::string fileNameFoot;
    static const std::string fileNameCompressed;
    static const std::string fileNameRaw;
    static const char fileNameSeparator = '_';
    static const std::string compressionExt;

    static const int bitsPerPixel [ Streamer::N_CHANNELS ];

    // Norpix
    static const unsigned short norpixString[ NORPIX_STRING_LENGTH ];
    static const unsigned int norpixVar = 0xFEED;
	static const int norpixVer = SEQ_VER;
	static const int norpixHeaderSize = SEQ_HEADER_SIZE;
    static const int norpixOrigin = 0;
    static const int norpixBPS = 8;
    static const char norpixDesc[ NORPIX_DESC_LENGTH ];


    // Objects
    QFile seqFile;
    QDataStream *seqFileStream;
    int totalFrames;
    int width;
    int height;
    Streamer::Channels streamChannel;
    bool compressed;
    std::vector<unsigned char> compressionBuffer;
	


    // Helper functions
    void makeEmptyHeader();
    void writeHeader( int width, int height, int bpp_num );
    int hexCharToDecimal( char ch );
    int hexToDec( const std::string &hex );
};