#ifndef QSIMPLECONSOLE_H
#define QSIMPLECONSOLE_H

#include <QPlainTextEdit>
#include <QTime>

#include <deque>

class QSimpleConsole : public QPlainTextEdit
{
	Q_OBJECT

	Q_PROPERTY(QString prompt READ prompt WRITE setPrompt)
	Q_PROPERTY(bool timestampEnabled READ isTimestampEnabled WRITE setTimestampEnabled)
	Q_PROPERTY(int scrollbackLines READ scrollbackLines WRITE setScrollbackLines)

public:
	explicit QSimpleConsole(QWidget *parent = nullptr);

	QString prompt() const { return promptText; }
	void setPrompt(const QString &prompt);

	// The text typed after the prompt. Reading it leaves the console untouched.
	QString currentCommand() const;
	void setCurrentCommand(const QString &command);

	bool isTimestampEnabled() const { return timestampEnabled; }
	void setTimestampEnabled(bool enabled) { timestampEnabled = enabled; }

	// Caps the output lines kept above the input line. A value of 0 or less
	// keeps every line, which is the default. Capping the scrollback also
	// disables undo/redo, which is how QPlainTextEdit's block limit behaves.
	void setScrollbackLines(int lines);
	int scrollbackLines() const;

	bool canCut() const;
	bool canPaste() const;

	// Deprecated in 1.2.0, to be removed in 2.0.0.
	[[deprecated("use currentCommand()")]] QString getCommand() const
	{
		return currentCommand();
	}
	[[deprecated("use setTimestampEnabled()")]] void setTimestamp(bool state)
	{
		setTimestampEnabled(state);
	}

public slots:
	void appendOutput(const QString &output);

	// Empties the console and draws a fresh prompt. This hides, rather than
	// overrides, QPlainTextEdit's non-virtual clear() slot: reaching the widget
	// through a QPlainTextEdit pointer clears the prompt along with everything
	// else. History is kept; clearHistory() drops that separately.
	void clear();
	void clearHistory();

signals:
	void commandEntered(const QString &command);

protected:
	void keyPressEvent(QKeyEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;
	void insertFromMimeData(const QMimeData *source) override;

private:
	// The boundary between the prompt and what the user may edit is derived
	// from the document rather than cached, so it cannot go stale.
	int inputStart() const;
	bool canEditAt(const QTextCursor &cursor) const;
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

	void addToHistory(const QString &command);

	QString promptText = "> ";
	std::deque<QString> history;
	QString historyDraft;
	uint32_t historySizeMax = 32;
	int32_t historyIndex = -1;
	bool handlingSubmittedCommand = false;
	bool timestampEnabled = false;
};

#endif // QSIMPLECONSOLE_H
