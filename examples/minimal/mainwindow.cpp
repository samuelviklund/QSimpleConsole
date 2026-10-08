#include "mainwindow.h"

#include "qsimpleconsole.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
	QSimpleConsole *console = new QSimpleConsole(this);
	console->setTimestampEnabled(true);

	connect(console, &QSimpleConsole::commandEntered, console, &QSimpleConsole::appendOutput);

	setCentralWidget(console);
}
