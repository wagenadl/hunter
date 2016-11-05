/** 
 * @file player.cpp
 * @brief Video player
 *
 * @author Santiago Navonne
 *
 * This contains the video player for project hunter.
 * This currently doesn't work! 
 * @TODO The player should be capable of reading a set of four files at a time
 */

// Project includes
#include "player.h"
#include "exceptions.h"

// C++
#include <thread>
#include <chrono>

using namespace std;

/**
 * @brief Application Constructor
 * @param parent The parent QObject
 */
Player::Player(QObject *parent)
	: QThread(parent)
{
    // Start off stopped
	stop = true;
}  

/**
 * @brief Application Destructor
 * @arg None.
 */
Player::~Player(void)
{
    // TODO
}

/**
 * @brief Load a video to be played
 * @param filename The path to the file to be loaded.
 * @returns Whether the video could be loaded.
 */
bool Player::loadVideo(string filename) 
{
    // Attempt to open the file TODO

    // Check whether the file was successfully opened TODO

    return false;
}

/**
 * @brief Play the loaded video
 * @arg None.
 * @returns void
 */
void Player::Play()
{
    // TODO
}

/**
 * @brief Run the video player
 * @arg None.
 * @returns void
 */
void Player::run()
{
    QImage img;

    // Compute delay between frames
	int delay = 1000 / frameRate;
    
    // Keep reading frames while not stopped.
	while( !stop ){
        // TODO

        // Release the hounds
		emit processedImage( img );
        
        // And wait for the next image
		this->msleep( delay );
	}
}

/**
 * @brief Stop playing avideo
 * @arg None.
 * @returns void
 */
void Player::Stop()
{
	stop = true;
}

/**
 * @brief Sleep for some number of milliseconds
 * @param ms Number of milliseconds to sleep for.
 * @returns void
 */
void Player::msleep(int ms)
{
    // TODO use real library...
	int ts =  ms;
    // Sleep
	std::this_thread::sleep_for( std::chrono::milliseconds( ts ) );
}

/**
 * @brief Accessor for stop value.
 * @arg None.
 * @returns Whether the player is stopped.
 */
bool Player::isStopped() const {
	return this->stop;
}

