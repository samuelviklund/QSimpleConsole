#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
	console = new QSimpleConsole(this);
	console->setTimestampEnabled(true);

	connect(console, &QSimpleConsole::commandEntered, console, &QSimpleConsole::appendOutput);

	setCentralWidget(console);
}

MainWindow::~MainWindow() {}
