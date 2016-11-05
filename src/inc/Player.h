/** 
 * @file player.h
 * @brief Video player
 *
 * @author Santiago Navonne
 *
 * This contains the definitions for the video player for project hunter.
 */

#pragma once

// Libraries
#include <QTCore/QMutex>
#include <QTCore/QThread>
#include <QTGui/QImage>
#include <QTCore/QWaitCondition>

// C++
#include <string>

class Player : public QThread
{
	Q_OBJECT
private:
	bool stop;
	bool stream;
	QImage frame;
	int frameRate;

signals:
	void processedImage( const QImage &image );
protected:
	void run();
	void msleep( int ms );
public:
	Player( QObject *parent = 0 );
	~Player();

	bool loadVideo( std::string filename );
	void Play();
	void Stop();
	bool isStopped() const;

};

