/** 
 * @file hunter.cpp
 * @brief Main Application
 *
 * @author Santiago Navonne
 *
 * This contains the main application for project hunter. The implementation currently
 * makes use of Qt. 
 */

// Project includes
#include "hunter.h"
#include "camera_controller.h"
#include "exceptions.h"

// Libraries
#include <QTCore/QtDebug>
#include <QTCore/QTimer>
#include <QTCore/QObject>
#include <QTWidgets/QFileDialog>
#include <QTWidgets/QMessageBox>
#include <QTCore/QThread>

// C++
#include <omp.h>
#include <iostream>
#include <fstream>
#include <stdint.h>

using namespace std;

/**
 * @brief Application Constructor
 * @param parent The parent QWidget.
 */
Hunter::Hunter( QWidget *parent )
	: QMainWindow( parent ),
      cameras ({ { { true, // Point Grey Top
                     &ui.viewPGT,
                     &ui.recordPGT,
                     &ui.compressedPGT,
                     &ui.fpsPGT,
                     &ui.shutterPGT,
                     &ui.gainPGT,
                     &ui.brightnessPGT,
                     &ui.roiXPGT,
                     &ui.roiYPGT,
                     &ui.roiWPGT,
                     &ui.roiHPGT,
                     &ui.canvasPGT },
                   { true, // Point Grey Front
                     &ui.viewPGF,
                     &ui.recordPGF,
                     &ui.compressedPGF,
                     &ui.fpsPGF,
                     &ui.shutterPGF,
                     &ui.gainPGF,
                     &ui.brightnessPGF,
                     &ui.roiXPGF,
                     &ui.roiYPGF,
                     &ui.roiWPGF,
                     &ui.roiHPGF,
                     &ui.canvasPGF },
                   { false, // Intel Color
                     &ui.viewColor,
                     &ui.recordColor,
                     &ui.compressedColor,
                     NULL,
                     NULL,
                     NULL,
                     NULL,
                     &ui.roiXColor,
                     &ui.roiYColor,
                     &ui.roiWColor,
                     &ui.roiHColor,
                     &ui.canvasColor },
                   { false, // Intel Depth
                     &ui.viewDepth,
                     &ui.recordDepth,
                     &ui.compressedDepth,
                     NULL,
                     NULL,
                     NULL,
                     NULL,
                     &ui.roiXDepth,
                     &ui.roiYDepth,
                     &ui.roiWDepth,
                     &ui.roiHDepth,
                     &ui.canvasDepth } } }) // Awkward syntax due to CS2536
{
	// Instantiate application objects
	player = new Player();
	cc = new CameraController();
	streamer = new Streamer( cc );

    // Create look up tables
	createLUTs();
	
	// Connect signals and slots
    qRegisterMetaType< CameraController::Cameras > ( "CameraController::Cameras" );
	QObject::connect( player,
		              SIGNAL( processedImage( QImage ) ),
					  this, 
					  SLOT( updatePlayerUI( QImage ) ) ); // This is used by the player

	QObject::connect( streamer,
		              SIGNAL( updateCamera( CameraController::Cameras, QImage ) ),
		              this, 
					  SLOT( updateStreamForCamera( CameraController::Cameras, QImage ) ),
                      Qt::QueuedConnection );

	QObject::connect(streamer,
		SIGNAL(updateFPSMeter()),
		this,
		SLOT(updateFPSMeter()),
		Qt::QueuedConnection);	
		
	QObject::connect( streamer,
		              SIGNAL( onStopSavingEvent() ),
		              this, 
		              SLOT( updateRecordButtonOnStopSaving() ) );

	// Instantiate and bind timer to this
	QTimer *timer = new QTimer( this );
	QObject::connect( timer,
		              SIGNAL( timeout() ), 
					  this, 
					  SLOT( timerEvent() ) );
	timer->start( TIMER_PERIOD );
	
	// Initialize variables
	recording = false;  // We're not recording
	calibrationInitialized = false; // Calibration hasn't been initialized yet

	// Setup UI
	ui.setupUi( this );

    // TODO: The following code is commented out to prevent cameras from being enable before they work properly
	// View checkboxes are all checked
	//ui.viewPGT->setCheckState( Qt::CheckState::Checked );
	//ui.viewPGF->setCheckState( Qt::CheckState::Checked );
	//ui.viewDepth->setCheckState( Qt::CheckState::Checked );
	//ui.viewColor->setCheckState( Qt::CheckState::Checked );

	// Record checkboxes are all checked
	//ui.recordPGT->setCheckState( Qt::CheckState::Checked );
	//ui.recordPGF->setCheckState( Qt::CheckState::Checked );
	//ui.recordColor->setCheckState( Qt::CheckState::Checked );
	//ui.recordDepth->setCheckState( Qt::CheckState::Checked );

	// Intel depth image starts off grey
	ui.greyDepth->setChecked( true );
		
	// Start streaming video
	streamer->run();
	setSideBar();

	// Initialize working directory on status bar
	statusBarWorkingDir = new QLabel( this );
	ui.statusBar->addPermanentWidget(statusBarWorkingDir, 1);

	// And load configuration
	QFile file( DEFAULT_CONFIG_FILE );
	if (file.open( QIODevice::ReadOnly )) {
		loadConfig( DEFAULT_CONFIG_FILE );
		file.close();
	}
	else { // No configuration found: override working directory
        setWorkingDirectory( &QString(), false );
    }

	// This should have been done already
	// cc->setValue(CameraController::Cameras::PointGreyTop, CameraController::CameraProperties::FPS, 30.0);
}

/**
 * @brief Application destructor
 */
Hunter::~Hunter()
{
	// Save configuration
	saveConfig( DEFAULT_CONFIG_FILE );
	
	// And clean up
	delete streamer;
    delete cc;
    delete player;
}

/**
 * @brief Generate look-up tables for color conversions
 * @arg None.
 * @return void.
 */
void Hunter::createLUTs()
{
	// Create look up table for grayscale -> RGB conversion
	for ( uint i = 0; i < 256; i++ ) grayscaleLUT.append( qRgb( i, i, i ) );

	// Create look up table for grayscale -> JET conversion
	float fDeltaR = 0.0, fDeltaG = 0.0, fDeltaB = 0.0156;
	float fRed = 0, fGreen = 0, fBlue = 0.5;
	uint uRed, uGreen, uBlue;

	for ( uint i = 0; i < 256; i++ ) {
		if (i == 32) {
			fDeltaR = 0.0; fDeltaG = 0.0156; fDeltaB = 0.0;
		}
		else if (i == 96) {
			fDeltaR = 0.0156; fDeltaG = 0.0; fDeltaB = -0.0156;
		}
		else if (i == 160) {
			fDeltaR = 0.0; fDeltaG = -0.0156; fDeltaB = 0.0;
		}
		else if (i == 224) {
			fDeltaR = -0.0156; fDeltaG = 0.0; fDeltaB = 0.0;
		}

		fRed += fDeltaR; fGreen += fDeltaG; fBlue += fDeltaB;

		uRed = static_cast<uint>( fRed * 255 );
		uGreen = static_cast<uint>( fGreen * 255 );
		uBlue = static_cast<uint>( fBlue * 255 );

		jetLUT.append( qRgb( uRed, uGreen, uBlue ) );
	}
}

/**
 * @brief Slot for PGT Apply Settings button.
 * @arg None.
 * @returns void.
 */
void Hunter::on_applyButtonPGT_clicked() 
{
	// Check the values
	checkPGValues( ui.fpsPGT,
			       ui.shutterPGT,
				   ui.gainPGT,
				   ui.brightnessPGT,
				   ui.roiXPGT,
				   ui.roiYPGT,
				   ui.roiWPGT,
				   ui.roiHPGT,
		           cc,
        		   CameraController::Cameras::PointGreyTop,
				   streamer );

}

/**
* @brief Slot for PGF Apply Settings button.
* @arg None.
* @returns void.
*/
void Hunter::on_applyButtonPGF_clicked() 
{
	// Check the values
	checkPGValues( ui.fpsPGF,
				   ui.shutterPGF,
				   ui.gainPGF,
				   ui.brightnessPGF,
				   ui.roiXPGF,
				   ui.roiYPGF,
				   ui.roiWPGF,
				   ui.roiHPGF,
				   cc,
				   CameraController::Cameras::PointGreyFront,
				   streamer );
}

/**
* @brief Check Point Grey Camera configuration values.
* @arg None.
* @returns void.
*
* Verifies that the entered Point Grey Camera configuration values are valid and
* applies the new settings.
*/
void Hunter::checkPGValues( QTextEdit* editFPS,
						    QTextEdit* editShutter,
						    QTextEdit* editGain,
						    QTextEdit* editBrightness,
						    QTextEdit* editX,
						    QTextEdit* editY,
						    QTextEdit* editWidth,
						    QTextEdit* editHeight,
						    CameraController* cameraController,
						    CameraController::Cameras camera,
						    Streamer* stream )
{
	// If the switch option is set, apply to the other camera
	if (ui.usb0PGF->isChecked()) {
		camera = camera == CameraController::Cameras::PointGreyTop ? CameraController::Cameras::PointGreyFront : CameraController::Cameras::PointGreyTop;
	}
	
	// Assume everything is good
	bool valid = true;

	// Convert everything to a float
	float fFPS = editFPS->toPlainText().toFloat();
	float fShutter = editShutter->toPlainText().toFloat();
	float fGain = editGain->toPlainText().toFloat();
	float fBrightness = editBrightness->toPlainText().toFloat();
	float fX = editX->toPlainText().toFloat();
	float fY = editY->toPlainText().toFloat();
	float fW = editWidth->toPlainText().toFloat();
	float fH = editHeight->toPlainText().toFloat();

	// Validate FPS value
	if ( fFPS < 0 || fFPS > PG_FPS_MAX )
	{
		textBoxEntryError( editFPS );
		valid = false;
	}
	// Validate shutter value
	if ( fShutter <= 0 || fShutter > PG_SHUTTER_MAX ) 
	{
		textBoxEntryError( editShutter );
		valid = false;
	}
	// Validate gain value
	if ( fGain <= 0 || fGain > PG_GAIN_MAX)
	{
		textBoxEntryError( editGain );
		valid = false;
	}
	// Validate ROI
	if ( fX < 0 || fX >= PG_RES_X )
	{
		textBoxEntryError( editX );
		valid = false;
	}
	if ( fY < 0 || fY >= PG_RES_Y )
	{
		textBoxEntryError( editY );
		valid = false;
	}
	if ( fW <= 0 || (fX + fW) > PG_RES_X )
	{
		textBoxEntryError( editWidth );
		valid = false;
	}
	if ( fH <= 0 || (fY + fH) > PG_RES_Y )
	{
        textBoxEntryError( editHeight );
		valid = false;
	}


	if ( valid ) { // Everything was good

		// Reset textboxes style
        resetTextBoxStyle( editFPS );
        resetTextBoxStyle( editShutter );
        resetTextBoxStyle( editGain );
        resetTextBoxStyle( editBrightness );
        resetTextBoxStyle( editX );
        resetTextBoxStyle( editY );
        resetTextBoxStyle( editWidth );
        resetTextBoxStyle( editHeight );

		// Apply camera settings

        try
        {
		    cameraController->setValue( camera, CameraController::CameraProperties::FPS, fFPS );
        }
        catch ( exception_t e )
        {
            if ( e == EXCEPTION_PG ); // TODO
        }
        try
        {
			cameraController->setValue(camera, CameraController::CameraProperties::Shutter, fShutter);
        }
        catch ( exception_t e )
        {
            if ( e == EXCEPTION_PG ); // TODO
        }
        try
        {
			cameraController->setValue(camera, CameraController::CameraProperties::Gain, fGain);
        }
        catch ( exception_t e )
        {
            if ( e == EXCEPTION_PG ); // TODO
        }
        try
        {
			cameraController->setValue(camera, CameraController::CameraProperties::Brightness, fBrightness);
        }
        catch ( exception_t e )
        {
            if ( e == EXCEPTION_PG ); // TODO
        }

		// Apply ROI values
        stream->setROI( camera, fX, fY, fW, fH );
	}
}

/**
* @brief Slot for Depth Apply Settings button.
* @arg None.
* @returns void.
*/
void Hunter::on_applyButtonDepth_clicked()
{
	applyDepth();
}

/**
* @brief Apply depth configuration values.
* @arg None.
* @returns void.
*
* Verifies that the entered Depth Camera configuration values are valid and
* applies the new settings.
*/
void Hunter::applyDepth() 
{
	// Convert values to numbers
	float fX = ui.roiXDepth->toPlainText().toFloat();
	float fY = ui.roiYDepth->toPlainText().toFloat();
	float fW = ui.roiWDepth->toPlainText().toFloat();
	float fH = ui.roiHDepth->toPlainText().toFloat();
	int iMaxDist = ui.maxDistDepth->toPlainText().toInt();

	// Assume everything was valid
	bool valid = true;

	if ( fX < 0 || fX >= DEPTH_X_MAX )
	{
		textBoxEntryError( ui.roiXDepth );
		valid = false;
	}
	if ( fY < 0 || fY >= DEPTH_Y_MAX )
	{
		textBoxEntryError( ui.roiYDepth );
		valid = false;
	}
	if ( fH <= 0 || (fY + fH) > DEPTH_Y_MAX )
	{
		textBoxEntryError( ui.roiHDepth );
		valid = false;
	}
	if ( fW <= 0 || (fX + fW) > DEPTH_X_MAX )
	{
		textBoxEntryError( ui.roiWDepth );
		valid = false;
	}
	if ( iMaxDist < DEPTH_DIST_MIN ) {
        textBoxEntryError( ui.maxDistDepth );
		valid = false;
	}

	if ( valid ) { // Everything was good
		// Reset textboxes style
        resetTextBoxStyle( ui.roiXDepth );
        resetTextBoxStyle( ui.roiYDepth );
        resetTextBoxStyle( ui.roiHDepth );
        resetTextBoxStyle( ui.roiWDepth );
        resetTextBoxStyle( ui.maxDistDepth );
		
		// And apply settings
		streamer->maxDepthMM = iMaxDist;
        streamer->setROI( CameraController::Cameras::Depth, fX, fY, fW, fH );
	}
}

/**
* @brief Slot for Color Apply Settings button.
* @arg None.
* @returns void.
*/
void Hunter::on_applyButtonColor_clicked()
{
	applyColor();
}

/**
* @brief Apply color camera configuration values.
* @arg None.
* @returns void.
*
* Verifies that the entered color camera configuration values are valid and
* applies the new settings.
*/
void Hunter::applyColor()
{
	// Convert text values to floats
	float fX = ui.roiXColor->toPlainText().toFloat();
	float fY = ui.roiYColor->toPlainText().toFloat();
	float fW = ui.roiWColor->toPlainText().toFloat();
	float fH = ui.roiHColor->toPlainText().toFloat();

	// Assume everything was valid
	bool valid = true;

	if ( fX < 0 || fX >= COLOR_X_MAX )
	{
		textBoxEntryError( ui.roiXColor );
		valid = false;
	}
	if ( fY < 0 || fY >= COLOR_Y_MAX )
	{
		textBoxEntryError( ui.roiYColor );
		valid = false;
	}
	if ( fH <= 0 || (fY + fH) > COLOR_X_MAX )
	{
		textBoxEntryError( ui.roiHColor );
		valid = false;
	}
	if ( fW <= 0 || (fX + fW) > COLOR_Y_MAX )
	{
        textBoxEntryError( ui.roiWColor );
		valid = false;
	}

	if ( valid ) { // Everything was good
		// Reset textbox style
        resetTextBoxStyle(ui.roiXColor);
        resetTextBoxStyle(ui.roiYColor);
        resetTextBoxStyle(ui.roiHColor);
        resetTextBoxStyle(ui.roiWColor);

		// And apply settings
        streamer->setROI( CameraController::Cameras::Color, fX, fY, fW, fH );
	}
}

/**
* @brief Reset textbox style.
* @param textBox The textbox whose style should be reset.
* @returns void.
*/
void Hunter::resetTextBoxStyle( QTextEdit* textBox )
{
    textBox->setStyleSheet( BG_COLOR_WHITE );
}

/**
* @brief Highlight an entry error in a textbox
* @param textBox The textbox whose style should be changed.
* @returns void.
*/
void Hunter::textBoxEntryError( QTextEdit* textBox )
{
    textBox->setStyleSheet( BG_COLOR_LIGHT_RED );
}

/**
* @brief Update the player user interface.
* @param img The image to update with.
* @returns void.
* 
* Updates the player user interface with a new image
* Is this not redundant to updateStreamForCamera()? -Zack
*/
void Hunter::updatePlayerUI( QImage img )
{
	// Check that the image is not null; that'd be bad.
	if (!img.isNull())
	{
		// TODO Player should really update up to four streams at a time
		ui.canvasPGT->setAlignment( Qt::AlignCenter );
		ui.canvasPGT->setPixmap( QPixmap::fromImage( img ).scaled( ui.canvasPGT->size(),
			                                                          Qt::KeepAspectRatio, 
							                                          Qt::FastTransformation ) );
	}
}

/*
* @brief Update the FPS indicator that a new frame was received. 
* @returns void.
*/
void Hunter::updateFPSMeter()
{
	qDebug() << "got FPS update" << endl;;
	

	chrono::high_resolution_clock::time_point now = chrono::high_resolution_clock::now();
	int lastFrameMs = chrono::duration_cast<chrono::milliseconds>(now - previousFrameTime).count() % 1000000000;
	previousFrameTime = chrono::high_resolution_clock::now();

	if (lastFrameMs > 0) {
		float lastFrameFPS = 1000.0 / lastFrameMs;
		fpsEMA = emaWeight * lastFrameFPS + (1.0 - emaWeight) * fpsEMA;

		//QString text = QString("%1").arg(fpsEMA);
		QString text = QString::number(fpsEMA, 'f', 2);
		ui.fpsLabel->setText(text);
	}
}

/**
 * @brief Update streamer and UI based on a change in view check box.
 * @param cam Camera whose view checkbox was pressed.
 * @returns void.
 */
void Hunter::viewCheckBoxChangedFor( CameraController::Cameras cam )
{
    if ( cameras[ cam ].viewCheckBox[0]->isChecked() ) // Should enable the stream
		// Enable the stream
		streamer->startStreaming( cam );
	else // Should disable the stream
	{
		// Disable the stream
		streamer->stopStreaming( cam );
		if (cam == CameraController::Cameras::PointGreyFront || cam == CameraController::Cameras::PointGreyTop) {
			if (ui.usb0PGF->isChecked()) {
				cameras[CameraController::Cameras::PointGreyFront ? cam == CameraController::Cameras::PointGreyTop : CameraController::Cameras::PointGreyFront].canvas[0]->clear();
			}
			else {
				cameras[cam].canvas[0]->clear();
			}
		}
		else {
			cameras[cam].canvas[0]->clear();
		}
		
        //cameras[ cam ].canvas[0]->setStyleSheet( STREAM_DISABLED );
	}

    // Disable snapshot button if no camera is streaming
    bool streaming = false;
    for ( int camera = 0; camera < CameraController::Cameras::NUM_CAMERAS; camera++ )
    {
        if ( cameras[ cam ].viewCheckBox[0]->isChecked() )
        {
            streaming = true;
            break;
        }
    }
    ui.snapButton->setDisabled( !streaming );
}

/**
 * @brief Slot for Point Grey Top view checkbox change.
 * @arg None.
 * @returns void.
 */
void Hunter::on_viewPGT_stateChanged()
{
	viewCheckBoxChangedFor( CameraController::Cameras::PointGreyTop );
}

/**
* @brief Slot for Point Grey Front view checkbox change.
* @arg None.
* @returns void.
*/
void Hunter::on_viewPGF_stateChanged()
{
	viewCheckBoxChangedFor( CameraController::Cameras::PointGreyFront );
}

/**
* @brief Slot for Color view checkbox change.
* @arg None.
* @returns void.
*/
void Hunter::on_viewColor_stateChanged()
{
	viewCheckBoxChangedFor( CameraController::Cameras::Color );
}

/**
* @brief Slot for Depth view checkbox change.
* @arg None.
* @returns void.
*/
void Hunter::on_viewDepth_stateChanged()
{
	viewCheckBoxChangedFor( CameraController::Cameras::Depth );
}


/**
* @brief Update UI based on record checkbox change.
* @param cam The camera whose checkbox was changed.
* @returns void.
*/
void Hunter::recordCheckBoxChangedFor( CameraController::Cameras cam )
{
    // Get new configuration
    bool newRecord = cameras[cam].recordCheckBox[0]->isChecked();
    // Update UI
    cameras[cam].compressedCheckBox[0]->setDisabled( !newRecord );

}

/**
* @brief Slot for Point Grey Top record checkbox change.
* @arg None.
* @returns void.
*/
void Hunter::on_recordPGT_stateChanged()
{
	recordCheckBoxChangedFor( CameraController::Cameras::PointGreyTop );
}

/**
* @brief Slot for Point Grey Front record checkbox change.
* @arg None.
* @returns void.
*/
void Hunter::on_recordPGF_stateChanged()
{
	recordCheckBoxChangedFor( CameraController::Cameras::PointGreyFront );
}

/**
* @brief Slot for Color record checkbox change.
* @arg None.
* @returns void.
*/
void Hunter::on_recordColor_stateChanged()
{
	recordCheckBoxChangedFor( CameraController::Cameras::Color );
}

/**
* @brief Slot for Depth record checkbox change.
* @arg None.
* @returns void.
*/
void Hunter::on_recordDepth_stateChanged()
{
	recordCheckBoxChangedFor( CameraController::Cameras::Depth );
}


/**
* @brief Update streamer based on record compression choice.
* @param cam Camera whose compression settings were changed.
* @returns void.
*/
void Hunter::compressionCheckBoxChangedFor( CameraController::Cameras cam )
{
    streamer->setCompressed( cam, cameras[cam].compressedCheckBox[0]->isChecked() );
}
/**
* @brief Slot for Point Grey Top Camera JPEG checkbox.
* @arg None.
* @returns void
*/
void Hunter::on_compressedPGT_stateChanged()
{
	compressionCheckBoxChangedFor( CameraController::Cameras::PointGreyTop );
}
/**
* @brief Slot for Point Grey Top Camera JPEG checkbox.
* @arg None.
* @returns void.
*/
void Hunter::on_compressedPGF_stateChanged()
{
	compressionCheckBoxChangedFor( CameraController::Cameras::PointGreyFront );
}
/**
* @brief Slot for Point Grey Top Camera JPEG checkbox.
* @arg None.
* @returns void.
*/
void Hunter::on_compressedColor_stateChanged()
{
	compressionCheckBoxChangedFor( CameraController::Cameras::Color );
}
/**
* @brief Slot for Point Grey Top Camera JPEG checkbox.
* @arg None.
* @returns void.
*/
void Hunter::on_compressedDepth_stateChanged()
{
	compressionCheckBoxChangedFor( CameraController::Cameras::Depth );
}

/**
* @brief Slot for Start/Stop Recording button press.
* @arg None.
* @returns void.
*/
void Hunter::on_recordButton_clicked()
{
	// Check whether we want to record
	if ( ui.recordButton->text() == "Record" )
	{
		// Update UI
		ui.recordButton->setText( tr( "Stop" ) );
		ui.recordButton->setStyleSheet( "QPushButton { background-color: rgb(255, 50, 50) }" );
		ui.inputSwitchPG->setDisabled( true );
		ui.applyButtonPGT->setDisabled( true );
		ui.applyButtonPGF->setDisabled( true );
		ui.applyButtonColor->setDisabled( true );
		ui.applyButtonDepth->setDisabled( true );
		ui.clearButtonPGT->setDisabled( true );
		ui.clearButtonPGF->setDisabled( true );
		ui.clearButtonColor->setDisabled( true );
		ui.clearButtonDepth->setDisabled( true );

		// Start recording

		streamer->startRecording((**cameras[CameraController::Cameras::PointGreyTop].recordCheckBox).isChecked(),
								(**cameras[CameraController::Cameras::PointGreyFront].recordCheckBox).isChecked(),
								(**cameras[CameraController::Cameras::Color].recordCheckBox).isChecked(),
								(**cameras[CameraController::Cameras::Depth].recordCheckBox).isChecked() );
        recording = true;
		// Save the recording start time
		mStartTime = QDateTime::currentDateTime();
		
	}
	else if ( ui.recordButton->text() == "Stop" ) // We want to stop
	{
		// Update UI
		ui.recordButton->setText( tr( "Saving..." ) );
		ui.recordButton->setStyleSheet( "QPushButton { background-color: rgb(255, 50, 50) }" );
		ui.recordButton->setDisabled( true );

		// Actually stop recording
		streamer->stopRecording();
		recording = false;

		this_thread::sleep_for(std::chrono::milliseconds(1000)); // Wait for the file to hopefully finish saving
																 // Update UI
		ui.recordButton->setText(tr("Record"));
		ui.recordButton->setStyleSheet("QPushButton { background-color: rgb(50, 255, 50) }");
		ui.recordButton->setDisabled(false);


		ui.inputSwitchPG->setDisabled(false);
		ui.applyButtonPGT->setDisabled(false);
		ui.applyButtonPGF->setDisabled(false);
		ui.applyButtonColor->setDisabled(false);
		ui.applyButtonDepth->setDisabled(false);
		ui.clearButtonPGT->setDisabled(false);
		ui.clearButtonPGF->setDisabled(false);
		ui.clearButtonColor->setDisabled(false);
		ui.clearButtonDepth->setDisabled(false);

	}
	
}

/**
* @brief Slot for Stop Saving action.
* @arg None.
* @returns void.
*/
void Hunter::updateRecordButtonOnStopSaving()
{
	// Double-check that we were actually saving
	if ( ui.recordButton->text() == "Saving..." )
	{
		ui.recordButton->setText( tr( "Record" ) );
		ui.recordButton->setStyleSheet( "QPushButton { background-color: rgb(50, 255, 50); }" );
		
		ui.inputSwitchPG->setDisabled( false );

		// Enable buttons
		ui.recordButton->setDisabled( false );

		ui.inputSwitchPG->setDisabled( false );

		ui.applyButtonPGT->setDisabled( false );
		ui.applyButtonPGF->setDisabled( false );
		ui.applyButtonColor->setDisabled( false );
		ui.applyButtonDepth->setDisabled( false );

		ui.clearButtonPGT->setDisabled( false );
		ui.clearButtonPGF->setDisabled( false );
		ui.clearButtonColor->setDisabled( false );
		ui.clearButtonDepth->setDisabled( false );
	}
}

/**
* @brief Slot for Snap button click.
* @arg None.
* @returns void.
*/
void Hunter::on_snapButton_clicked()
{
    // Let streamer know.
    for ( int camera = 0; camera < CameraController::Cameras::NUM_CAMERAS; camera++ )
    {
        if ( cameras[ camera ].viewCheckBox[0]->isChecked() )
            streamer->saveSnapshot( (CameraController::Cameras) camera );
    }
}

/**
* @brief Slot for main timer expiration event.
* @arg None.
* @returns void.
*/
void Hunter::timerEvent()
{
    // Only do stuff if we were recording.
	if ( recording ) {
        // Compute time elapsed since we started recording
        // TODO see if we can use the time library for this
		long int time = mStartTime.msecsTo( QDateTime::currentDateTime() );
		int h = time / 1000 / 60 / 60;
		int m = (time / 1000 / 60) % 60;
		int s = (time / 1000) % 60;
		int ms = time % 1000;
		QString diff = QString( "%1 h  %2 m  %3.%4 s" ).arg( h ).arg( m ).arg( s ).arg( ms );
        // And update timer label.
		ui.timerLabel->setText( diff );			
	}
}


/**
* @brief Slot for window close event
* @param event The triggering event.
* @returns void.
*
* If currently recording, warn the user about this and let them abort.
*
*/
void Hunter::closeEvent( QCloseEvent* event )
{
    // If user is not recording, everything is good.
	if ( recording == false ) {
		event->accept();
	}
	else { // User is recording; warn them.
		QMessageBox warningBox;
		warningBox.setText( "Warning: Recording in progress." );
		QAbstractButton *saveButton = warningBox.addButton( tr( "Save and exit" ), QMessageBox::YesRole );
		QAbstractButton *cancelButton = warningBox.addButton( tr( "Cancel" ), QMessageBox::YesRole );
		warningBox.exec();

        // Proceed as necessary
		if ( warningBox.clickedButton() == saveButton ) { // User wants to exit
            // Stop recording
			streamer->stopRecording();
			recording = false;
            // And exit
			event->accept();
		}
		if ( warningBox.clickedButton() == cancelButton ) { // User wants to stay
            // Ignore close event
			event->ignore();
		}
	}
}

chrono::high_resolution_clock::time_point last_stream_update = chrono::high_resolution_clock::now();
bool timer_set = false;
/**
* @brief Update a camera stream
* @param cam Camera whose stream should be updated
* @param image The image to update with.
* @returns void.
*/
void Hunter::updateStreamForCamera( CameraController::Cameras cam, 
                                    QImage image )
{
	if (cam == CameraController::Cameras::PointGreyFront && streamer->isPGswitched) {
		cam = CameraController::Cameras::PointGreyTop;
	}
	else if (cam == CameraController::Cameras::PointGreyTop && streamer->isPGswitched) {
		cam = CameraController::Cameras::PointGreyFront;
	}

	QLabel* canvas = cameras[cam].canvas[0];

	// Ensure that we do something only if the incoming image is valid
	if (!image.isNull())
	{
		if (cam != CameraController::Cameras::Depth) // Most cameras behave the same
		{
			canvas->setAlignment(Qt::AlignCenter);
			canvas->setPixmap( QPixmap::fromImage( image ).scaled( canvas->size(),
								Qt::KeepAspectRatio,
								Qt::FastTransformation ) );
		}
		else // Depth camera is a little special
		{
			canvas->setAlignment(Qt::AlignCenter);

			QPixmap originalPixmap = QPixmap::fromImage(image, Qt::AutoColor).scaled(canvas->size(),
				Qt::KeepAspectRatio,
				Qt::FastTransformation);
				
			// Convert image and compute sizes
			pixmap = QPixmap::fromImage(image, Qt::AutoColor).scaled(canvas->size(),
				Qt::KeepAspectRatio,
				Qt::FastTransformation);
			QSize sizeLabel = canvas->size();
			QSize sizePixmap = pixmap.size();
			margin = (QPoint(sizeLabel.width(), sizeLabel.height()) - QPoint(sizePixmap.width(), sizePixmap.height())) / 2;
			QImage pixmapToImage = originalPixmap.toImage();

			// Set everything up to draw that pixmap
			QPainter painter(&pixmap);
			painter.setBrush(QBrush(Qt::blue));
			painter.setPen(QColor(Qt::blue));
			painter.setFont(QFont("Arial", 12));

			// Display calibration points if necessary
			if (ui.menu_calibrationMode->isChecked())
			{
				if (!calibrationInitialized) // Calibration needs to be initialized
				{
					// Initialize calibration points in a square, 1/5 from a side, 1/2 along the other side
					calibrationPoints[0].x = pixmap.width() / 2;
					calibrationPoints[0].y = pixmap.height() / 5;

					calibrationPoints[1].x = pixmap.width() / 5;
					calibrationPoints[1].y = pixmap.height() / 2;

					calibrationPoints[2].x = pixmap.width() / 2;
					calibrationPoints[2].y = pixmap.height() * 4 / 5;

					calibrationPoints[3].x = pixmap.width() * 4 / 5;
					calibrationPoints[4].y = pixmap.height() / 2;

					// And mark calibration as already initialized
					calibrationInitialized = true;
				}

				// Draw calibration points
				QPoint calibrationPointsQ[4];
				calibrationPointsQ[0] = QPoint(calibrationPoints[0].x, calibrationPoints[0].y);
				calibrationPointsQ[1] = QPoint(calibrationPoints[1].x, calibrationPoints[1].y);
				calibrationPointsQ[2] = QPoint(calibrationPoints[2].x, calibrationPoints[2].y);
				calibrationPointsQ[3] = QPoint(calibrationPoints[3].x, calibrationPoints[3].y);
				for (int i = 0; i < 4; i++)
					painter.drawEllipse(calibrationPointsQ[i],
					CALIBRATION_POINT_SIZE,
					CALIBRATION_POINT_SIZE);

				// Compute and draw calibration values
				int pixelsDiff_0 = getDepthValue(pixmapToImage, calibrationPointsQ[0]) -
					getDepthValue(pixmapToImage, calibrationPointsQ[2]);
				painter.setPen(QColor(abs(pixelsDiff_0), 255 - abs(pixelsDiff_0), 0));
				painter.drawLine(calibrationPointsQ[0], calibrationPointsQ[2]);
				painter.drawText(QPoint(calibrationPoints[2].x + CALIBRATION_TEXT_OFFSET,
					calibrationPoints[2].y - CALIBRATION_TEXT_OFFSET),
					QString::number(pixelsDiff_0));

				int pixelsDiff_1 = getDepthValue(pixmapToImage, calibrationPointsQ[1]) -
					getDepthValue(pixmapToImage, calibrationPointsQ[3]);
				painter.setPen(QColor(abs(pixelsDiff_1), 255 - abs(pixelsDiff_1), 0));
				painter.drawLine(calibrationPointsQ[1], calibrationPointsQ[3]);
				painter.drawText(QPoint(calibrationPoints[3].x + CALIBRATION_TEXT_OFFSET,
					calibrationPoints[3].y - CALIBRATION_TEXT_OFFSET),
					QString::number(pixelsDiff_1));

			}

			// Show calibration
			ui.canvasDepth->setPixmap(pixmap);
		}
	}
	else
		canvas->clear();
}

/**
* @brief Populate Point Grey sidebar configuration values.
* @arg None.
* @returns void.
*/
void Hunter::setSideBar()
{
	// Set Point Grey Top FPS value
    try {
        resetTextBoxWithFloat( ui.fpsPGT, 
                               cc->getValue( CameraController::Cameras::PointGreyTop, 
                                             CameraController::CameraProperties::FPS ), 
                               SIDEBAR_PRECISION );
    }
    catch ( exception_t e )
    {
        if ( e == EXCEPTION_PG_CONFIG ); // TODO
        if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
    }

	// Set Point Grey Top shutter value
    try {
        resetTextBoxWithFloat( ui.shutterPGT, 
                               cc->getValue( CameraController::Cameras::PointGreyTop, 
                                             CameraController::CameraProperties::Shutter ), 
                               SIDEBAR_PRECISION );
            }
    catch ( exception_t e )
    {
        if ( e == EXCEPTION_PG_CONFIG ); // TODO
        if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
    }

	// Set Point Grey Top gain value
    try {
        resetTextBoxWithFloat( ui.gainPGT, 
                               cc->getValue( CameraController::Cameras::PointGreyTop, 
                                             CameraController::CameraProperties::Gain ), 
                               SIDEBAR_PRECISION );
    }
    catch ( exception_t e )
    {
        if ( e == EXCEPTION_PG_CONFIG ); // TODO
        if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
    }

	// Set Point Grey Top brightness value
    try {
        resetTextBoxWithFloat( ui.brightnessPGT, 
                               cc->getValue( CameraController::Cameras::PointGreyTop, 
                                             CameraController::CameraProperties::Brightness ), 
                               SIDEBAR_PRECISION );
    }
    catch ( exception_t e )
    {
        if ( e == EXCEPTION_PG_CONFIG ); // TODO
        if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
    }

	// Set Point Grey Front FPS value
    try {
        resetTextBoxWithFloat( ui.fpsPGF, 
                               cc->getValue( CameraController::Cameras::PointGreyFront, 
                                             CameraController::CameraProperties::FPS ), 
                               SIDEBAR_PRECISION );
    }
    catch ( exception_t e )
    {
        if ( e == EXCEPTION_PG_CONFIG ); // TODO
        if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
    }

	// Set Point Grey Front Shutter value
    try {
        resetTextBoxWithFloat( ui.shutterPGF, 
                               cc->getValue( CameraController::Cameras::PointGreyFront, 
                                             CameraController::CameraProperties::Shutter ), 
                               SIDEBAR_PRECISION );
    }
    catch ( exception_t e )
    {
        if ( e == EXCEPTION_PG_CONFIG ); // TODO
        if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
    }

	// Set Point Grey Front Gain value
    try {
        resetTextBoxWithFloat( ui.gainPGF, 
                               cc->getValue( CameraController::Cameras::PointGreyFront, 
                                             CameraController::CameraProperties::Gain ), 
                               SIDEBAR_PRECISION );
    }
    catch ( exception_t e )
    {
        if ( e == EXCEPTION_PG_CONFIG ); // TODO
        if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
    }

	// Set Point Grey Front Shutter value
    try {
        resetTextBoxWithFloat( ui.brightnessPGF, 
                               cc->getValue( CameraController::Cameras::PointGreyFront, 
                                             CameraController::CameraProperties::Brightness ), 
                               SIDEBAR_PRECISION );
    }
    catch ( exception_t e )
    {
        if ( e == EXCEPTION_PG_CONFIG ); // TODO
        if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
    }
}

/**
* @brief Reset a textbox with a float.
* @param textEdit Textbox to be reset.
* @param newValue Value to be inserted into textBox.
* @param precision Floating precision to be displayed.
* @returns void.
*/
void Hunter::resetTextBoxWithFloat( QTextEdit* textEdit, 
                                    float newValue, 
                                    int precision )
{
	textEdit->setText( QString::number( newValue, 'f', precision ) );
    resetTextBoxStyle( textEdit );
}

/**
* @brief Clear configuration fields for a given camera.
* @param cam The camera whose fields should be cleared.
* @returns void.
*/
void Hunter::clearFieldsFor( CameraController::Cameras cam )
{
    CameraControls theCamera = cameras[ cam ];
    // Point Grey cameras have some extra controls
    if ( theCamera.isPointGrey )
    {
        int cameraNum = ui.usb0PGF->isChecked();
        try 
        {
            resetTextBoxWithFloat( theCamera.FPSTextBox[0], 
                                   cc->getValue( ( CameraController::Cameras ) cameraNum,
                                                 CameraController::CameraProperties::FPS ), 
                                   SIDEBAR_PRECISION );
        }
        catch ( exception_t e )
        {
            if ( e == EXCEPTION_PG_CONFIG ); // TODO
            if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
        }
        try 
        {
            resetTextBoxWithFloat( theCamera.shutterTextBox[0], 
                                   cc->getValue( ( CameraController::Cameras ) cameraNum,
                                                 CameraController::CameraProperties::Shutter ), 
                                   SIDEBAR_PRECISION );
        }
        catch ( exception_t e )
        {
            if ( e == EXCEPTION_PG_CONFIG ); // TODO
            if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
        }
        try 
        {
            resetTextBoxWithFloat( theCamera.gainTextBox[0], 
                                   cc->getValue( ( CameraController::Cameras ) cameraNum,
                                                 CameraController::CameraProperties::Gain ), 
                                   SIDEBAR_PRECISION );
        }
        catch ( exception_t e )
        {
            if ( e == EXCEPTION_PG_CONFIG ); // TODO
            if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
        }
        try 
        {
            resetTextBoxWithFloat( theCamera.brightnessTextBox[0], 
                                   cc->getValue( ( CameraController::Cameras ) cameraNum,
                                                 CameraController::CameraProperties::Brightness ), 
                                   SIDEBAR_PRECISION );
        }
        catch ( exception_t e )
        {
            if ( e == EXCEPTION_PG_CONFIG ); // TODO
            if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
        }
    }

	int test = streamer->getROI(cam, Streamer::ROICoordinates::W);

    resetTextBoxWithFloat( theCamera.ROIX[0], streamer->getOriginalROI( cam, Streamer::ROICoordinates::X ), 0 );
	resetTextBoxWithFloat( theCamera.ROIY[0], streamer->getOriginalROI( cam, Streamer::ROICoordinates::Y ), 0 );
	resetTextBoxWithFloat( theCamera.ROIWidth[0], streamer->getOriginalROI( cam, Streamer::ROICoordinates::W ), 0 );
	resetTextBoxWithFloat( theCamera.ROIHeight[0], streamer->getOriginalROI( cam, Streamer::ROICoordinates::H ), 0 );
}
/**
* @brief Slot for Point Grey Top Camera Clear button.
* @arg None.
* @returns void.
*/
void Hunter::on_clearButtonPGT_clicked()
{ 
    clearFieldsFor( CameraController::Cameras::PointGreyTop );
}

/**
* @brief Slot for Point Grey Front Camera Clear button.
* @arg None.
* @returns void.
*/
void Hunter::on_clearButtonPGF_clicked()
{
    clearFieldsFor( CameraController::Cameras::PointGreyFront );
}

/**
* @brief Slot for Depth Camera Clear button.
* @arg None.
* @returns void.
*/
void Hunter::on_clearButtonDepth_clicked()
{
	resetTextBoxWithFloat( ui.maxDistDepth, streamer->maxDepthMM, 0 );
	clearFieldsFor( CameraController::Cameras::Depth );
}

/**
* @brief Slot for Color Camera Clear button.
* @arg None.
* @returns void.
*/
void Hunter::on_clearButtonColor_clicked()
{
    clearFieldsFor( CameraController::Cameras::Color );
}

void Hunter::updateTextBox( QTextEdit* textBox,
                            QString old,
                            int maxLength )
{
    QString text = textBox->toPlainText();
    if ( text.length() > maxLength || !FLOAT_VALIDATION.exactMatch( text ) )
        textBox->textCursor().deletePreviousChar();
	if ( text == old )
        resetTextBoxStyle( textBox );
	else 
        textBox->setStyleSheet( BG_COLOR_LIGHT_BLUE );
}

/**
* @brief Slot for Point Grey Top Camera FPS value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_fpsPGT_textChanged() 
{
	int cameraNum = ui.usb0PGF->isChecked();
    try
    {
	    updateTextBox( ui.fpsPGT, 
                       QString::number( cc->getValue( ( CameraController::Cameras ) cameraNum, 
                                                      CameraController::CameraProperties::FPS ), 
                                        'f', 
                                        SIDEBAR_PRECISION ),
                       5 );
    }
    catch ( exception_t e )
    {
        if ( e == EXCEPTION_PG_CONFIG ); // TODO
        if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
    }
}

/**
* @brief Slot for Point Grey Top Camera shutter value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_shutterPGT_textChanged()
{
	int cameraNum = ui.usb0PGF->isChecked();
    try
    {
	    updateTextBox( ui.shutterPGT, 
                       QString::number( cc->getValue( ( CameraController::Cameras ) cameraNum, 
                                                      CameraController::CameraProperties::Shutter ), 
                                        'f', 
                                        SIDEBAR_PRECISION ),
                       5 );
    }
    catch ( exception_t e )
    {
        if ( e == EXCEPTION_PG_CONFIG ); // TODO
        if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
    }
}

/**
* @brief Slot for Point Grey Top Camera gain value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_gainPGT_textChanged()
{
	int cameraNum = ui.usb0PGF->isChecked();
    try
    {
	    updateTextBox( ui.gainPGT, 
                       QString::number( cc->getValue( ( CameraController::Cameras ) cameraNum, 
                                                      CameraController::CameraProperties::Gain ), 
                                        'f', 
                                        SIDEBAR_PRECISION ),
                       5 );
    }
    catch ( exception_t e )
    {
        if ( e == EXCEPTION_PG_CONFIG ); // TODO
        if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
    }
}

/**
* @brief Slot for Point Grey Top Camera brightness value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_brightnessPGT_textChanged()
{
	int cameraNum = ui.usb0PGF->isChecked();
    try {
	    updateTextBox( ui.brightnessPGT, 
                       QString::number( cc->getValue( ( CameraController::Cameras ) cameraNum, 
                                                      CameraController::CameraProperties::Brightness ), 
                                        'f', 
                                        SIDEBAR_PRECISION ),
                       5 );
    }
    catch ( exception_t e )
    {
        if ( e == EXCEPTION_PG_CONFIG ); // TODO
        if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
    }
}

/**
* @brief Slot for Point Grey Top Camera ROI X coordinate value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_roiXPGT_textChanged()
{
	updateTextBox( ui.roiXPGT, 
                   QString::number( streamer->getROI( CameraController::Cameras::PointGreyTop, 
                                                      Streamer::ROICoordinates::X ), 
                                    'f', 
                                    1 ),
                   4 );
}

/**
* @brief Slot for Point Grey Top Camera ROI Y coordinate value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_roiYPGT_textChanged()
{
	updateTextBox( ui.roiYPGT, 
                   QString::number( streamer->getROI( CameraController::Cameras::PointGreyTop, 
                                                      Streamer::ROICoordinates::Y ), 
                                    'f', 
                                    1 ),
                   4 );
}

/**
* @brief Slot for Point Grey Top Camera ROI width value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_roiWPGT_textChanged()
{
	updateTextBox( ui.roiWPGT, 
                   QString::number( streamer->getROI( CameraController::Cameras::PointGreyTop, 
                                                      Streamer::ROICoordinates::W ), 
                                    'f', 
                                    1 ),
                   4 );
}

/**
* @brief Slot for Point Grey Top Camera ROI height value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_roiHPGT_textChanged()
{
	updateTextBox( ui.roiHPGT, 
                   QString::number( streamer->getROI( CameraController::Cameras::PointGreyTop, 
                                                      Streamer::ROICoordinates::H ), 
                                    'f', 
                                    1 ),
                   4 );
}

/**
* @brief Slot for Point Grey Front Camera FPS value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_fpsPGF_textChanged()
{
	int cameraNum = ui.usb0PGT->isChecked();
    try
    {
	    updateTextBox( ui.fpsPGF, 
                       QString::number( cc->getValue( ( CameraController::Cameras ) cameraNum, 
                                                      CameraController::CameraProperties::FPS ), 
                                        'f', 
                                        SIDEBAR_PRECISION ),
                       5 );
    }
    catch ( exception_t e )
    {
        if ( e == EXCEPTION_PG_CONFIG ); // TODO
        if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
    }
}

/**
* @brief Slot for Point Grey Front Camera shutter value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_shutterPGF_textChanged()
{
	int cameraNum = ui.usb0PGT->isChecked();
    try
    {
	    updateTextBox( ui.shutterPGF, 
                       QString::number( cc->getValue( ( CameraController::Cameras ) cameraNum, 
                                                      CameraController::CameraProperties::Shutter ), 
                                        'f', 
                                        SIDEBAR_PRECISION ),
                       5 );
    }
    catch ( exception_t e )
    {
        if ( e == EXCEPTION_PG_CONFIG ); // TODO
        if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
    }
}

/**
* @brief Slot for Point Grey Front Camera gain value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_gainPGF_textChanged()
{
	int cameraNum = ui.usb0PGT->isChecked();
    try
    {
	    updateTextBox( ui.gainPGF, 
                       QString::number( cc->getValue( ( CameraController::Cameras ) cameraNum, 
                                                      CameraController::CameraProperties::Gain ), 
                                        'f', 
                                        SIDEBAR_PRECISION ),
                       5 );
    }
    catch ( exception_t e )
    {
        if ( e == EXCEPTION_PG_CONFIG ); // TODO
        if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
    }
}

/**
* @brief Slot for Point Grey Front Camera brightness value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_brightnessPGF_textChanged()
{
	int cameraNum = ui.usb0PGT->isChecked();
    try
    {
	    updateTextBox( ui.brightnessPGF, 
                       QString::number( cc->getValue( ( CameraController::Cameras ) cameraNum, 
                                                      CameraController::CameraProperties::Brightness ), 
                                        'f', 
                                        SIDEBAR_PRECISION ),
                       5 );
    }
    catch ( exception_t e )
    {
        if ( e == EXCEPTION_PG_CONFIG ); // TODO
        if ( e == EXCEPTION_PG_INVALID_CAM ); // TODO
    }
}

/**
* @brief Slot for Point Grey Front Camera ROI X coordinate value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_roiXPGF_textChanged()
{
	updateTextBox( ui.roiXPGF, 
                   QString::number( streamer->getROI( CameraController::Cameras::PointGreyFront, 
                                                      Streamer::ROICoordinates::X ), 
                                    'f', 
                                    1 ),
                   4 );
}

/**
* @brief Slot for Point Grey Front Camera ROI Y coordinate value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_roiYPGF_textChanged()
{
	updateTextBox( ui.roiYPGF, 
                   QString::number( streamer->getROI( CameraController::Cameras::PointGreyFront, 
                                                      Streamer::ROICoordinates::Y ), 
                                    'f', 
                                    1 ),
                   4 );
}

/**
* @brief Slot for Point Grey Front Camera ROI width value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_roiWPGF_textChanged()
{
	updateTextBox( ui.roiWPGF, 
                   QString::number( streamer->getROI( CameraController::Cameras::PointGreyFront, 
                                                      Streamer::ROICoordinates::W ), 
                                    'f', 
                                    1 ),
                   4 );
}

/**
* @brief Slot for Point Grey Front Camera ROI height value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_roiHPGF_textChanged()
{
	updateTextBox( ui.roiHPGF, 
                   QString::number( streamer->getROI( CameraController::Cameras::PointGreyFront, 
                                                      Streamer::ROICoordinates::H ), 
                                    'f', 
                                    1 ),
                   4 );
}

/**
* @brief Slot for Depth Camera maximum distance value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_maxDistDepth_textChanged() 
{
    updateTextBox( ui.maxDistDepth, 
                   QString::number( streamer->maxDepthMM , 'f', 1 ),
                   4 );
}

/**
* @brief Slot for Color Camera ROI X coordinate value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_roiXColor_textChanged()
{
    updateTextBox( ui.roiXColor, 
                   QString::number( streamer->getROI( CameraController::Cameras::Color, 
                                                      Streamer::ROICoordinates::X ) , 
                                    'f', 
                                    1 ),
                   4 );
}

/**
* @brief Slot for Color Camera ROI Y coordinate value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_roiYColor_textChanged()
{
    updateTextBox( ui.roiYColor, 
                   QString::number( streamer->getROI( CameraController::Cameras::Color, 
                                                      Streamer::ROICoordinates::Y ) , 
                                    'f', 
                                    1 ),
                   4 );
}

/**
* @brief Slot for Color Camera ROI width value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_roiWColor_textChanged()
{
    updateTextBox( ui.roiWColor, 
                   QString::number( streamer->getROI( CameraController::Cameras::Color, 
                                                      Streamer::ROICoordinates::W ) , 
                                    'f', 
                                    1 ),
                   4 );
}

/**
* @brief Slot for Color Camera ROI height value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_roiHColor_textChanged()
{
    updateTextBox( ui.roiHColor, 
                   QString::number( streamer->getROI( CameraController::Cameras::Color, 
                                                      Streamer::ROICoordinates::H ) , 
                                    'f', 
                                    1 ),
                   4 );
}

/**
* @brief Slot for Depth Camera ROI X coordinate value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_roiXDepth_textChanged()
{
    updateTextBox( ui.roiXDepth, 
                   QString::number( streamer->getROI( CameraController::Cameras::Depth, 
                                                      Streamer::ROICoordinates::X ) , 
                                    'f', 
                                    1 ),
                   4 );
}

/**
* @brief Slot for Depth Camera ROI Y coordinate value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_roiYDepth_textChanged()
{
    updateTextBox( ui.roiYDepth, 
                   QString::number( streamer->getROI( CameraController::Cameras::Depth, 
                                                      Streamer::ROICoordinates::Y ) , 
                                    'f', 
                                    1 ),
                   4 );
}

/**
* @brief Slot for Depth Camera ROI width value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_roiWDepth_textChanged()
{
    updateTextBox( ui.roiWDepth, 
                   QString::number( streamer->getROI( CameraController::Cameras::Depth, 
                                                      Streamer::ROICoordinates::W ) , 
                                    'f', 
                                    1 ),
                   4 );
}

/**
* @brief Slot for Depth Camera ROI height value changed.
* @arg None.
* @returns void.
*/
void Hunter::on_roiHDepth_textChanged()
{
    updateTextBox( ui.roiHDepth, 
                   QString::number( streamer->getROI( CameraController::Cameras::Depth, 
                                                      Streamer::ROICoordinates::H ) , 
                                    'f', 
                                    1 ),
                   4 );
}


/**
* @brief Slot for Load Configuration action.
* @arg None.
* @returns void.
*/
void Hunter::on_menu_loadConfig_triggered()
{
    // Get a file name
	QString fileName = QFileDialog::getOpenFileName( this, 
                                                     tr( "Open File" ), 
                                                     QString(), 
                                                     tr( "XML files (*.xml)" ) );

    // Double-check that a file was in fact selected
	if ( !fileName.isEmpty() ) {
        // Make sure the file can be opened
		QFile file( fileName );
		if ( !file.open( QIODevice::ReadOnly ) ) {
			QMessageBox::critical( this, 
                                   tr( "Error" ), 
                                   tr( "Could not open file" ) );
			return;
		}
		file.close();

        // And load the configuration
		loadConfig( fileName );
	
	}
}

/**
* @brief Load configuration from a file.
* @param fileName Path to the file containing the configuration.
* @returns void.
*/
void Hunter::loadConfig( QString fileName )
{
    bool isRecord;

	// Load XML file
	pugi::xml_document doc; 
    doc.load_file( fileName.toStdString().c_str() );

    // Find attributes in file
	pugi::xml_node cameraSetting = doc.first_child();
	pugi::xml_node pointGreyTop = cameraSetting.find_child_by_attribute( "pointGrey", "location", "top" );
	pugi::xml_node pointGreyFront = cameraSetting.find_child_by_attribute( "pointGrey", "location", "front" );
	pugi::xml_node intelColor = cameraSetting.child( "intelColor" );
	pugi::xml_node intelDepth = cameraSetting.child( "intelDepth" );
	pugi::xml_node record;
	pugi::xml_attribute method;
    pugi::xml_attribute usb;

	// Check and set values on user interface
	// Working directory
    string dir = cameraSetting.child_value( "currentWorkingDirectory" );
    setWorkingDirectory( &QString( dir.c_str() ), true );

	// Point Grey Top Camera
	usb = pointGreyTop.attribute( "usb" );
	if ( usb ) 
        ui.usb0PGT->setChecked( !strcmp( usb.value(), "0" ) );

	ui.viewPGT->setChecked( !strcmp( pointGreyTop.child_value( "view" ), "true" ) );
	//ui.usb0PGF->isChecked(); // TODO Unnecessary?
	isRecord = !strcmp( pointGreyTop.child_value( "record" ), "true" );
	ui.recordPGT->setChecked( isRecord );
	ui.compressedPGT->setDisabled( !isRecord );
	record = pointGreyTop.child( "record" );
	method = record.attribute( "method" );
	ui.compressedPGT->setChecked( ( isRecord ) && ( method ) && ( !strcmp( method.value(), "jpeg" ) ) );
	ui.fpsPGT->setText( QString( pointGreyTop.child_value( "frameRate" ) ) );
	ui.shutterPGT->setText( QString( pointGreyTop.child_value( "shutterSpeed") ) );
	ui.gainPGT->setText( QString( pointGreyTop.child_value( "gain" ) ) );
	ui.brightnessPGT->setText(QString(pointGreyTop.child_value( "brightness" ) ) );
	setROIvalues( pointGreyTop.child( "roi" ),
		          ui.roiXPGT,
		          ui.roiYPGT,
		          ui.roiWPGT,
		          ui.roiHPGT );

	// PG Front
	usb = pointGreyFront.attribute("usb");
	if ( usb ) 
        ui.usb0PGF->setChecked( !strcmp( usb.value(), "0" ) );
	ui.viewPGF->setChecked( !strcmp( pointGreyFront.child_value( "view" ), "true" ) );
	isRecord = !strcmp( pointGreyFront.child_value( "record" ), "true" );
	ui.recordPGF->setChecked( isRecord );
	ui.compressedPGF->setDisabled( !isRecord );
	record = pointGreyFront.child( "record" );
	method = record.attribute( "method" );
	ui.compressedPGF->setChecked( ( isRecord ) && ( method ) && ( !strcmp( method.value(), "jpeg" ) ) );
	ui.fpsPGF->setText( QString( pointGreyFront.child_value( "frameRate" ) ) );
	ui.shutterPGF->setText( QString( pointGreyFront.child_value( "shutterSpeed" ) ) );
	ui.gainPGF->setText( QString( pointGreyFront.child_value( "gain" ) ) );
	ui.brightnessPGF->setText( QString( pointGreyFront.child_value( "brightness" ) ) );
	setROIvalues( pointGreyFront.child( "roi" ),
		          ui.roiXPGF,
		          ui.roiYPGF,
		          ui.roiWPGF,
		          ui.roiHPGF );

	// Intel Color
	ui.viewColor->setChecked( !strcmp( intelColor.child_value( "view" ), "true" ) );
	isRecord = !strcmp( intelColor.child_value( "record" ), "true" );
	ui.recordColor->setChecked( isRecord );
	ui.compressedColor->setDisabled( !isRecord );
	record = intelColor.child( "record" );
	method = record.attribute( "method" );
	ui.compressedColor->setChecked( ( isRecord ) && ( method ) && ( !strcmp( method.value(), "jpeg" ) ) );
	ui.recordColor->setChecked( !strcmp( intelColor.child_value( "record" ), "true" ) );
	setROIvalues( intelColor.child( "roi" ),
		          ui.roiXColor,
		          ui.roiYColor,
		          ui.roiWColor,
		          ui.roiHColor);

	// Intel Depth
	ui.viewDepth->setChecked( !strcmp( intelDepth.child_value( "view" ), "true" ) );
	isRecord = !strcmp( intelDepth.child_value( "record" ), "true" );
	ui.recordDepth->setChecked( isRecord );
	ui.compressedDepth->setDisabled( !isRecord );
	record = intelDepth.child( "record" );
	method = record.attribute( "method" );
	ui.compressedDepth->setChecked( ( isRecord ) && ( method ) && (!strcmp( method.value(), "jpeg" ) ) );
	ui.maxDistDepth->setText( QString( intelDepth.child_value( "maxValue" ) ).toStdString().c_str() );
	setROIvalues( intelDepth.child( "roi" ),
		          ui.roiXDepth,
		          ui.roiYDepth,
		          ui.roiWDepth,
		          ui.roiHDepth);

	// Apply values
	checkPGValues( ui.fpsPGT,
		           ui.shutterPGT,
		           ui.gainPGT,
		           ui.brightnessPGT,
		           ui.roiXPGT,
		           ui.roiYPGT,
		           ui.roiWPGT,
		           ui.roiHPGT,
		           cc,
		           CameraController::Cameras::PointGreyTop,
		           streamer );

	checkPGValues( ui.fpsPGF,
		           ui.shutterPGF,
		           ui.gainPGF,
		           ui.brightnessPGF,
		           ui.roiXPGF,
		           ui.roiYPGF,
		           ui.roiWPGF,
		           ui.roiHPGF,
		           cc,
		           CameraController::Cameras::PointGreyFront,
		           streamer );

	applyDepth();

	applyColor();

}

/**
* @brief Load Region-Of-Interest values from XML.
* @param roi XML Node for the ROI
* @param x X-coordinate of ROI
* @param y Y-coordinate of ROI
* @param width Width of ROI
* @param height Height of ROI.
* @returns void.
*/
void Hunter::setROIvalues( pugi::xml_node roi, 
                           QTextEdit* x, 
                           QTextEdit* y, 
                           QTextEdit* width, 
                           QTextEdit* height )
{
	x->setText( QString( roi.child_value( "x" ) ) );
	y->setText( QString( roi.child_value( "y" ) ) );
	width->setText( QString( roi.child_value( "width" ) ) );
	height->setText( QString( roi.child_value( "height" ) ) );
}

/**
* @brief Slot for Save Configuration action.
* @arg None.
* @returns void.
*/
void Hunter::on_menu_saveConfig_triggered()
{
    // Get file name of configuration file.
	QString fileName = QFileDialog::getSaveFileName( this, 
                                                     tr( "Save File" ), 
                                                     QString(), 
                                                     tr( "XML Files (*.xml)" ) );

    // Check that file can be opened
	if ( !fileName.isEmpty() ) {
		QFile file( fileName );
		if (!file.open( QIODevice::WriteOnly ) ) {
			QMessageBox::critical( this, 
                                   tr( "Error" ), 
                                   tr( "Could not save file" ) );
			return;
		}
		file.close();
		
        // And save configuration
		saveConfig( fileName );		
	}
}

/**
* @brief Save configuration to a file.
* @param fileName Path to the destination configuration file.
* @returns void.
*/
void Hunter::saveConfig( QString fileName ) 
{
	// Generate XML document
	pugi::xml_document doc;
	pugi::xml_node decl = doc.prepend_child( pugi::node_declaration );
	decl.append_attribute( "version" ) = "1.0";
	decl.append_attribute( "encoding" ) = "UTF-8";

    // Organize camera settings
	pugi::xml_node cameraSettings = doc.append_child("cameraSettings");

	// Save working directory
	pugi::xml_node cwd = cameraSettings.append_child( "currentWorkingDirectory" );
	cwd.append_child( pugi::node_pcdata ).set_value( workingDir.c_str() );

	// Point Grey Top Camera
	getPGvalues( &cameraSettings,
		         ui.usb0PGT,
		         ui.viewPGT,
		         ui.recordPGT,
		         ui.compressedPGT,
		         ui.fpsPGT,
		         ui.shutterPGT,
		         ui.gainPGT,
		         ui.brightnessPGT,
		         ui.roiXPGT,
		         ui.roiYPGT,
		         ui.roiWPGT,
		         ui.roiHPGT,
		         "top" );

	// Point Grey Front Camera
	getPGvalues( &cameraSettings,
		         ui.usb0PGF,
		         ui.viewPGF,
		         ui.recordPGF,
		         ui.compressedPGF,
		         ui.fpsPGF,
		         ui.shutterPGF,
		         ui.gainPGF,
		         ui.brightnessPGF,
		         ui.roiXPGF,
		         ui.roiYPGF,
		         ui.roiWPGF,
		         ui.roiHPGF,
		         "front" );

	// Intel Color
	pugi::xml_node nodeIntelColor = cameraSettings.append_child( "intelColor" );
	pugi::xml_node nodeViewColor = nodeIntelColor.append_child( "view" );
	bool viewSelected = ui.viewColor->isChecked();
	nodeViewColor.append_child( pugi::node_pcdata ).set_value( viewSelected ? "true" : "false" );
	pugi::xml_node nodeRecordColor = nodeIntelColor.append_child( "record" );
	bool recordSelected = ui.recordColor->isChecked();
	pugi::xml_attribute attributeRecordColor = nodeRecordColor.append_attribute( "method" );
	if ( recordSelected && ui.compressedColor->isChecked() ) 
        attributeRecordColor.set_value( "jpeg" );
	else 
        attributeRecordColor.set_value( "raw" );
	nodeRecordColor.append_child( pugi::node_pcdata ).set_value(recordSelected ? "true" : "false" );
	pugi::xml_node nodeRoiColor = nodeIntelColor.append_child( "roi" );
	getROIvalues( &nodeRoiColor, ui.roiXColor, ui.roiYColor, ui.roiWColor, ui.roiHColor );

	// Intel Depth
	pugi::xml_node nodeIntelDepth = cameraSettings.append_child( "intelDepth" );
	pugi::xml_node nodeViewDepth = nodeIntelDepth.append_child( "view" );
	viewSelected = ui.viewDepth->isChecked();
	nodeViewDepth.append_child( pugi::node_pcdata ).set_value(viewSelected ? "true" : "false" );
	pugi::xml_node nodeRecordDepth = nodeIntelDepth.append_child( "record" );
	recordSelected = ui.recordDepth->isChecked();
	pugi::xml_attribute attributeRecordDepth = nodeRecordDepth.append_attribute( "method" );
	if ( recordSelected && ui.compressedDepth->isChecked() ) 
        attributeRecordDepth.set_value( "jpeg" );
	else 
        attributeRecordDepth.set_value( "raw" );
	nodeRecordDepth.append_child( pugi::node_pcdata ).set_value( recordSelected ? "true" : "false" );
	pugi::xml_node nodeMaxDist = nodeIntelDepth.append_child( "maxValue" );
	nodeMaxDist.append_child( pugi::node_pcdata ).set_value( ui.maxDistDepth->toPlainText().toStdString().c_str() );
	pugi::xml_node nodeRoiDepth = nodeIntelDepth.append_child( "roi" );
	getROIvalues( &nodeRoiDepth, ui.roiXDepth, ui.roiYDepth, ui.roiWDepth, ui.roiHDepth );

	// Save XML file
	doc.save_file( fileName.toStdString().c_str() );
}

/**
* @brief Get and save Point Grey Camera configuration values to file.
* @param cameraSettings Node within XML file where settings should be saved.
* @arc @c usb Value of USB radio button
* @arc @c view Value of View checkbox
* @arc @c record Value of Record checkbox
* @arc @c jpeg Value of JPEG checkbox
* @arc @c frameRate Value of Frame Rate textbox
* @arc @c shutterSpeed Value of Shutter Speed textbox
* @arc @c gain Value of Gain textbox
* @arc @c brightness Value of Brightness textbox
* @arc @c x Value of ROI X textbox
* @arc @c y Value of ROI Y textbox
* @arc @c width Value of ROI width textbox
* @arc @c height Value of ROI height textbox
* @arc @c location Description of PG camera. @c front or @c top.
* @returns void.
*/
void Hunter::getPGvalues( pugi::xml_node* cameraSettings,
						  QRadioButton* usb,
						  QCheckBox* view,
						  QCheckBox* record,
						  QCheckBox* jpeg,
						  QTextEdit* frameRate,
						  QTextEdit* shutterSpeed,
						  QTextEdit* gain,
						  QTextEdit* brightness,
						  QTextEdit* x, 
						  QTextEdit* y, 
						  QTextEdit* width, 
						  QTextEdit* height,
						  std::string location )
{
    // Create child node for this camera
	pugi::xml_node nodePointGrey = cameraSettings->append_child( "pointGrey" );
    // And save data
	nodePointGrey.append_attribute( "location" ) = location.c_str();
	nodePointGrey.append_attribute( "usb" ) = usb->isChecked() ? "0" : "1";
	pugi::xml_node nodeView = nodePointGrey.append_child( "view" );
	nodeView.append_child( pugi::node_pcdata ).set_value( view->isChecked() ? "true" : "false" );
	pugi::xml_node nodeRecord = nodePointGrey.append_child( "record" );
	bool recordSelected = record->isChecked();
	pugi::xml_attribute attributeRecord = nodeRecord.append_attribute( "method" );
	if ( recordSelected && jpeg->isChecked() ) 
        attributeRecord.set_value( "jpeg" );
	else 
        attributeRecord.set_value( "raw" );
	nodeRecord.append_child( pugi::node_pcdata ).set_value( recordSelected ? "true" : "false" );
	pugi::xml_node nodeFrameRate = nodePointGrey.append_child( "frameRate" );
	nodeFrameRate.append_child( pugi::node_pcdata ).set_value( frameRate->toPlainText().toStdString().c_str() );
	pugi::xml_node nodeShutterSpeed = nodePointGrey.append_child( "shutterSpeed" );
	nodeShutterSpeed.append_child( pugi::node_pcdata ).set_value( shutterSpeed->toPlainText().toStdString().c_str() );
	pugi::xml_node nodeGain = nodePointGrey.append_child( "gain" );
	nodeGain.append_child( pugi::node_pcdata ).set_value( gain->toPlainText().toStdString().c_str() );
	pugi::xml_node nodeBrightness = nodePointGrey.append_child( "brightness" );
	nodeBrightness.append_child( pugi::node_pcdata ).set_value( brightness->toPlainText().toStdString().c_str() );
	pugi::xml_node nodeRoi = nodePointGrey.append_child( "roi" );
	getROIvalues( &nodeRoi, x, y, width, height );
}

/**
* @brief Get and save Region-Of-Interest configuration values to file.
* @param nodeRoi Node within XML file where settings should be saved.
* @arc @c x Value of ROI X textbox
* @arc @c y Value of ROI Y textbox
* @arc @c width Value of ROI width textbox
* @arc @c height Value of ROI height textbox
* @returns void.
*/
void Hunter::getROIvalues( pugi::xml_node* nodeRoi, 
						   QTextEdit* x, 
						   QTextEdit* y, 
						   QTextEdit* width, 
						   QTextEdit* height )
{
    
	pugi::xml_node nodeX = nodeRoi->append_child( "x" );
	nodeX.append_child( pugi::node_pcdata ).set_value( x->toPlainText().toStdString().c_str() );
	pugi::xml_node nodeY = nodeRoi->append_child( "y" );
	nodeY.append_child( pugi::node_pcdata).set_value( y->toPlainText().toStdString().c_str() );
	pugi::xml_node nodeWidth = nodeRoi->append_child( "width" );
	nodeWidth.append_child( pugi::node_pcdata ).set_value( width->toPlainText().toStdString().c_str() );
	pugi::xml_node nodeHeight = nodeRoi->append_child( "height" );
	nodeHeight.append_child( pugi::node_pcdata ).set_value( height->toPlainText().toStdString().c_str() );
}

/**
* @brief Slot for Open Folder action.
* @arg None.
* @returns void.
*/
void Hunter::on_menu_outputFolder_triggered()
{
    // Get directory to open
	QString dir = QFileDialog::getExistingDirectory( this, 
                                                     tr( "Open Directory" ),
		                                             getenv( "HOME" ), 
                                                     ( QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks ) );
    // And set it
    setWorkingDirectory(&dir, true);

}

/**
* @brief Change the program's current working directory.
* @param dir The directory that should be used
* @param stream Whether the change should be passed on to the Streamer.
* @returns void.
*/
void Hunter::setWorkingDirectory( QString* dir,
                                  bool stream )
{
	if ( !dir->size() || !QDir( QString( *dir ) ).exists() )
		workingDir = QDir::currentPath().toStdString();
    else
        workingDir = dir->toStdString();
    statusBarWorkingDir->setText( QString( workingDir.c_str() ) );
    if ( stream )
	    streamer->setCurrentWorkingDir( workingDir );
}

/**
* @brief Slot for Point Grey Top button toggled.
* @arg None.
* @returns void.
*/
void Hunter::on_usb0PGT_toggled()
{
	streamer->isPGswitched = ui.usb0PGF->isChecked();
	
	// Apply the settings so that the correct settings are applied to the camera
	on_applyButtonPGF_clicked();
	on_applyButtonPGT_clicked();

	// swapCameraSettings(); // TODO?
}

/**
* @brief Swap the settings for the Point Grey Top and Front cameras.
* @arg None.
* @returns void.
*/
void Hunter::swapCameraSettings()
{
	// Get copy of settings
	bool view = ui.viewPGT->isChecked();
	bool record = ui.recordPGT->isChecked();
	QString frame = ui.fpsPGT->toPlainText();
	QString shutter = ui.shutterPGT->toPlainText();
	QString gain = ui.gainPGT->toPlainText();
	QString brightness = ui.brightnessPGT->toPlainText();
	QString x = ui.roiXPGT->toPlainText();
	QString y = ui.roiYPGT->toPlainText();
	QString h = ui.roiHPGT->toPlainText();
	QString w = ui.roiWPGT->toPlainText();
	
    // Overwrite settings
	ui.viewPGT->setChecked( ui.viewPGF->isChecked() );
	ui.recordPGT->setChecked( ui.recordPGF->isChecked() );
	ui.fpsPGT->setText( ui.fpsPGF->toPlainText() );
	ui.shutterPGT->setText( ui.shutterPGF->toPlainText() );
	ui.gainPGT->setText( ui.gainPGF->toPlainText() );
	ui.brightnessPGT->setText( ui.brightnessPGF->toPlainText() );
	ui.roiXPGT->setText( ui.roiXPGF->toPlainText() );
	ui.roiYPGT->setText( ui.roiYPGF->toPlainText() );
	ui.roiHPGT->setText( ui.roiHPGF->toPlainText() );
	ui.roiWPGT->setText( ui.roiWPGF->toPlainText() );

    // And then overwrite other with copy
	ui.viewPGF->setChecked( view );
	ui.recordPGF->setChecked( record );
	ui.fpsPGF->setText( frame );
	ui.shutterPGF->setText( shutter );
	ui.gainPGF->setText( gain );
	ui.brightnessPGF->setText( brightness );
	ui.roiXPGF->setText( x );
	ui.roiYPGF->setText( y );
	ui.roiHPGF->setText( h );
	ui.roiWPGF->setText( w );

}

/**
* @brief Get the depth value at some point within a depth camera picture
* @param image Image within which the depth value should be returned.
* @param point Point at which the depth value is requested.
* @returns Depth value at that point within that image.
*/
int Hunter::getDepthValue( QImage image, 
                           QPoint point )
{
    // Do some averaging for smoothness
	int totalGrey = 0;
	for ( int i = -CALIBRATION_POINT_SIZE; i <= CALIBRATION_POINT_SIZE; i++ )
    {
		for ( int j = -CALIBRATION_POINT_SIZE; j <= CALIBRATION_POINT_SIZE; j++ ) {
			totalGrey += qGray( image.pixel( point.x() + i, point.y() + j ) );
		}
    }
	return totalGrey / (4 * CALIBRATION_POINT_SIZE * (CALIBRATION_POINT_SIZE + 1) + 1);
}

/**
* @brief Slot for mouse press events.
* @param event The mouse press event that triggered this.
* @returns void.
* @note This event is only enabled within the depth picture.
*/
void Hunter::mousePressEvent( QMouseEvent *event )
{
    // Get location of click within depth picture
	QPointF pos = ui.canvasDepth->mapFrom( this, event->pos() );
	QPoint point = pos.toPoint() - margin;

    // Initialize to be able to report errors
	calibrationPointMoved = -1;

    // Only do something if we're calibrating
	if ( ui.menu_calibrationMode->isChecked() )
    {
		for ( int i = 0; i < 4; i++ )
		{
			int dx = point.x() - calibrationPoints[ i ].x;
			int dy = point.y() - calibrationPoints[ i ].y;
			if ( ( ( dx * dx ) + ( dy * dy ) ) <= ( CALIBRATION_POINT_SIZE * CALIBRATION_POINT_SIZE ) ) 
            {
				calibrationPointMoved = i;
				this->setCursor( Qt::ClosedHandCursor );
			}
		}
    }
}

/**
* @brief Slot for mouse move events.
* @param event The mouse move event that triggered this.
* @returns void.
* @note This event is only enabled within the depth picture.
*/
void Hunter::mouseMoveEvent( QMouseEvent *event )
{
    // Get location of move within depth picture
	QPoint pos = ui.canvasDepth->mapFrom( this, event->pos() ) - margin;

	if ( ( pos.x() < CALIBRATION_POINT_SIZE ) || 
         ( pos.y() < CALIBRATION_POINT_SIZE ) ||
		 ( ( pos.x() + CALIBRATION_POINT_SIZE ) >= pixmap.width() ) || 
         ( ( pos.y() + CALIBRATION_POINT_SIZE ) >= pixmap.height() ) )
		return;
	
	if ( ( calibrationPointMoved == 0 ) || ( calibrationPointMoved == 2 ) )
	{
		calibrationPoints[ 0 ].x = pos.x();
		calibrationPoints[ 2 ].x = pos.x();
		calibrationPoints[ calibrationPointMoved ].y = pos.y();
	}

	if ( ( calibrationPointMoved == 1 ) || ( calibrationPointMoved == 3 ) )
	{
		calibrationPoints[ 1 ].y = pos.y();
		calibrationPoints[ 3 ].y = pos.y();
		calibrationPoints[ calibrationPointMoved ].x = pos.x();
	}
}

/**
* @brief Slot for mouse release events
* @param event The mouse release event that triggered this.
* @returns void.
* @note This event is only enabled within the depth picture.
*/
void Hunter::mouseReleaseEvent( QMouseEvent *event )
{
	this->setCursor( Qt::ArrowCursor );
	calibrationPointMoved = -1;
}

/**
* @brief Slot for resize events
* @param event The resize event that triggered this.
* @returns void.
* @note This event is only enabled within the depth picture.
*/
void Hunter::resizeEvent( QResizeEvent *event )
{
	calibrationInitialized = false;
}