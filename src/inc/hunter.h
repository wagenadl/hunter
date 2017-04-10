/**
* @file hunter.h
* @brief Main Application Header
*
* @author Santiago Navonne
*
* This contains the definitions for the main application for project hunter.
*/

#pragma once

// Project includes
#include "player.h"
#include "streamer.h"
#include "ui_hunter.h"
#include "camera_controller.h"
#include "pugixml.hpp"

// Libaries
#include <QtWidgets/QMainWindow>
#include <QTGui/QCloseEvent>
#include <QTGui/QPainter>
#include <QTCore/QDateTime>

// C++
#include <array>

class Hunter : public QMainWindow
{
	Q_OBJECT

public:
	// Application constructor
	Hunter( QWidget *parent = 0 );
	~Hunter();

	// Set the initial camera settings on the side bar
	void setSideBar();

private slots:
	// UI signals
	void on_recordButton_clicked();
	void on_snapButton_clicked();

	void on_applyButtonPGT_clicked();
	void on_applyButtonPGF_clicked();
	void on_applyButtonDepth_clicked();
	void on_applyButtonColor_clicked();

	void on_viewPGT_stateChanged();
	void on_viewPGF_stateChanged();
	void on_viewDepth_stateChanged();
	void on_viewColor_stateChanged();

	void on_recordPGT_stateChanged();
	void on_recordPGF_stateChanged();
	void on_recordColor_stateChanged();
	void on_recordDepth_stateChanged();

	void on_compressedPGT_stateChanged();
	void on_compressedPGF_stateChanged();
	void on_compressedDepth_stateChanged();
	void on_compressedColor_stateChanged();

	void on_usb0PGT_toggled();

	void on_clearButtonPGT_clicked();
	void on_clearButtonPGF_clicked();
	void on_clearButtonDepth_clicked();
	void on_clearButtonColor_clicked();

	void on_menu_outputFolder_triggered();
	void on_menu_loadConfig_triggered();
	void on_menu_saveConfig_triggered();

	void on_fpsPGT_textChanged();
	void on_shutterPGT_textChanged();
	void on_gainPGT_textChanged();
	void on_brightnessPGT_textChanged();
	void on_roiXPGT_textChanged();
	void on_roiYPGT_textChanged();
	void on_roiWPGT_textChanged();
	void on_roiHPGT_textChanged();

	void on_fpsPGF_textChanged();
	void on_shutterPGF_textChanged();
	void on_gainPGF_textChanged();
	void on_brightnessPGF_textChanged();
	void on_roiXPGF_textChanged();
	void on_roiYPGF_textChanged();
	void on_roiWPGF_textChanged();
	void on_roiHPGF_textChanged();

	void on_roiXColor_textChanged();
	void on_roiYColor_textChanged();
	void on_roiWColor_textChanged();
	void on_roiHColor_textChanged();

	void on_roiXDepth_textChanged();
	void on_roiYDepth_textChanged();
	void on_roiWDepth_textChanged();
	void on_roiHDepth_textChanged();

	void on_maxDistDepth_textChanged();
	
    // Other signals
    void updatePlayerUI( QImage img );
	// Send signal that we have received a new frame and the FPS should be updated.
	void updateFPSMeter();
    void updateStreamForCamera( CameraController::Cameras cam, QImage image );
	void timerEvent();
	void updateRecordButtonOnStopSaving();

protected:
	// Window events
	void closeEvent( QCloseEvent* event );
	void Hunter::mousePressEvent( QMouseEvent *event );
	void Hunter::mouseMoveEvent( QMouseEvent *event );
	void Hunter::mouseReleaseEvent( QMouseEvent *event );
	void Hunter::resizeEvent( QResizeEvent * event );

private:
    // Types
    /** Controls related to a single camera. */
    typedef struct CameraControls_t
    {
        bool isPointGrey;                /**< Whether the camera is Point Grey. */
        QCheckBox** viewCheckBox;        /**< The camera's View check box. */
        QCheckBox** recordCheckBox;      /**< The camera's Record check box. */
        QCheckBox** compressedCheckBox;  /**< The camera's Compressed check box. */
        QTextEdit** FPSTextBox;          /**< The camera's FPS text box. */
        QTextEdit** shutterTextBox;      /**< The camera's Shutter text box. */
        QTextEdit** gainTextBox;         /**< The camera's Gain text box. */
        QTextEdit** brightnessTextBox;   /**< The camera's Brightness text box. */
        QTextEdit** ROIX;                /**< The camera's ROI X text box. */
        QTextEdit** ROIY;                /**< The camera's ROI Y text box. */
        QTextEdit** ROIWidth;            /**< The camera's ROI width text box. */
        QTextEdit** ROIHeight;           /**< The camera's ROI height text box. */
        QLabel** canvas;                 /**< The camera's drawing canvas. */
    } CameraControls;

    /** A 2D point. */
    struct Point
    {
        int x;    /**< Point's X coordinate. */
        int y;    /**< Point's Y coordinate. */
    };

    // Constants
    enum 
    {
        SIDEBAR_PRECISION = 1,           /**< Precision of float values displayed in the sidebar in decimal digits.  */
        TIMER_PERIOD = 1000,             /**< Period of the UI update timer in milliseconds. */
        CALIBRATION_POINT_SIZE = 7,      /**< Size of the calibration dot in pixels. */
        PG_FPS_MAX = 162,                /**< Maximum value for the Point Grey FPS control. */
        PG_SHUTTER_MAX = 3200,           /**< Maximum value for the Point Grey Shutter control. */
        PG_GAIN_MAX = 18,                /**< Maximum value for the Point Grey Gain control. */
        PG_RES_X = 1920,                 /**< Maximum value for the Point Grey Camera resolution width. */
        PG_RES_Y = 1200,                 /**< Maximum value for the Point Grey Camera resolution height. */
        DEPTH_X_MAX = 320,               /**< Maximum value for the Depth Camera resolution width. */
        DEPTH_Y_MAX = 240,               /**< Maximum value for the Depth Camera resolution height. */
        DEPTH_DIST_MIN = 255,            /**< Minimum value for the Depth Camera background distance. */
        COLOR_X_MAX = 480,               /**< Maximum value for the Color Camera resolution height. */
        COLOR_Y_MAX = 640,               /**< Maximum value for the Color Camera resolution width. */
        CALIBRATION_TEXT_OFFSET = 5,     /**< Distance between calibration dot and its text in pixels. */
    };

    // Functions
    void viewCheckBoxChangedFor( CameraController::Cameras cam );
    void recordCheckBoxChangedFor( CameraController::Cameras cam );
    void compressionCheckBoxChangedFor( CameraController::Cameras cam );
    void clearFieldsFor( CameraController::Cameras cam );

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
								Streamer* stream );

	void Hunter::setROIvalues( pugi::xml_node roi,
							   QTextEdit* x,
							   QTextEdit* y,
							   QTextEdit* width,
							   QTextEdit* height );

	void Hunter::getROIvalues( pugi::xml_node* nodeRoi, 
							   QTextEdit* x, 
							   QTextEdit* y, 
							   QTextEdit* width, 
							   QTextEdit* height);

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
							  std::string location );

    void Hunter::resetTextBoxStyle( QTextEdit* textBox );
    void Hunter::textBoxEntryError( QTextEdit* textBox );

	void Hunter::resetTextBoxWithFloat( QTextEdit* textEdit, 
		                                float newValue, 
										int precision );

    void Hunter::updateTextBox( QTextEdit* textBox,
                                QString old,
                                int maxLength );

    void Hunter::setWorkingDirectory( QString* dir,
                                      bool stream );
	void Hunter::loadConfig( QString fileName );
	void Hunter::saveConfig( QString fileName );
	void Hunter::swapCameraSettings();
	void Hunter::applyDepth();
	void Hunter::applyColor();
	int Hunter::getDepthValue( QImage image, QPoint point );
	void Hunter::createLUTs();

	// Objects
    std::array< CameraControls, CameraController::Cameras::NUM_CAMERAS > cameras; // Must use std::array due to CS2536
	Ui::HunterClass ui;
	Player* player;
	Streamer* streamer;
	CameraController* cc;
	QDateTime mStartTime; // TODO
	
	// EMA weight: larger -> shorter window
	float emaWeight = .01;
	// Holds the exponential moving average FPS
	float fpsEMA = 30.0;
	// Previous frame timestamp
	chrono::high_resolution_clock::time_point previousFrameTime;

	bool recording;

    struct Point calibrationPoints[ 4 ];
	bool calibrationInitialized;
	int calibrationPointMoved;


	// Style-sheet
	QString BG_COLOR_LIGHT_BLUE = "QTextEdit { background-color: rgb(75, 75, 255) }";
	QString BG_COLOR_LIGHT_RED = "QTextEdit { background-color: rgb(255, 75, 75) }";
	QString BG_COLOR_WHITE = "QTextEdit { background-color: rgb(255, 255, 255) }";
    QString STREAM_DISABLED = "QTextEdit { background-color: rgb(70, 70, 70); }";

	QString DEFAULT_CONFIG_FILE = "default_config.xml";

	// Input validation
	QRegExp FLOAT_VALIDATION = QRegExp("\\d+\\.?\\d*");
	QRegExp INT_VALIDATION = QRegExp("\\d+");

	std::string workingDir;
	QLabel* statusBarWorkingDir;

	// Look up table for grayscale -> gray RGB
	QVector<QRgb> grayscaleLUT;
	// Look up table for grayscale -> jet RGB
	QVector<QRgb> jetLUT;

	QPoint margin;

	QPixmap pixmap;
};
