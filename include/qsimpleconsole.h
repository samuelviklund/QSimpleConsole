#ifndef QSIMPLECONSOLE_H
#define QSIMPLECONSOLE_H

#include <QPlainTextEdit>

#include <deque>

class QSimpleConsole : public QPlainTextEdit
{
	Q_OBJECT

public:
	QSimpleConsole();

	void setPrompt(const QString &p);
	QString getCommand() const;
	void setCurrentCommand(const QString &command);

	bool canCut() const;
	bool canPaste() const;

public slots:
	void appendOutput(const QString &result);

signals:
	void commandEntered(const QString &command);

protected:
	void keyPressEvent(QKeyEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;

private:
	void handleCut(QKeyEvent *event);
	void handlePaste(QKeyEvent *event);
	void handleSelectAll(QKeyEvent *event);
	void handleUpKey(QKeyEvent *event);
	void handleDownKey(QKeyEvent *event);
	void handleLeftRightKey(QKeyEvent *event);
	void handleReturnKey(QKeyEvent *event);
	void handleDeleteKey(QKeyEvent *event);
	void handleBackspaceKey(QKeyEvent *event);
	void handleHomeKey(QKeyEvent *event);

	void pushHistoryCommand(const QString &command);

	QString prompt = "> ";
	int32_t promptEndPosition;
	std::deque<QString> history;
	uint32_t historySizeMax = 32;
	int32_t historyIndex = -1;
};

#endif // QSIMPLECONSOLE_H
