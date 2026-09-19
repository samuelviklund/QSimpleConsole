#ifndef QSIMPLECONSOLE_H
#define QSIMPLECONSOLE_H

#include <QPlainTextEdit>
#include <QTime>

#include <deque>

class QSimpleConsole : public QPlainTextEdit
{
	Q_OBJECT

public:
	QSimpleConsole();

	void setPrompt(const QString &p);
	QString getCommand();
	void setCurrentCommand(const QString &command);
	void setTimestamp(bool state) { useTimestamp = state; }

	bool canCut() const;
	bool canPaste() const;

public slots:
	void appendOutput(const QString &result);

signals:
	void commandEntered(const QString &command);

protected:
	void keyPressEvent(QKeyEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;
	void insertFromMimeData(const QMimeData *source) override;

private:
	// The boundary between the prompt and what the user may edit is derived
	// from the document rather than cached, so it cannot go stale.
	int promptEnd() const;
	bool isEditRegion(const QTextCursor &cursor) const;
	void moveCursorToInputEnd();

	void handleCut(QKeyEvent *event);
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
	std::deque<QString> history;
	QString historyDraft;
	uint32_t historySizeMax = 32;
	int32_t historyIndex = -1;
	bool isUserInput = false;
	bool useTimestamp = false;
};

#endif // QSIMPLECONSOLE_H
