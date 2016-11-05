/* 
 * main.cpp
 * Project hunter
 * Main entry point
 *
 * Santiago Navonne
 */
// Project includes
#include "hunter.h"

// Libraries
#include <QtWidgets/QApplication>

using namespace std;

// The entry point
int main(int argc, char *argv[])
{
	qRegisterMetaType<vector<QImage>>("vector<QImage>");
	QApplication app(argc, argv);
	Hunter w;
	w.show();
	return app.exec();
}
