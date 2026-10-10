#include "qsimpleconsole.h"

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QMenu>
#include <QMimeData>
#include <QRegularExpression>
#include <QStringList>
#include <QTextBlock>
#include <QTextStream>
#include <QTime>

#include <memory>

namespace {

/* True for the keys that would put text into the document. Copy, undo and the
 * navigation keys must not be treated as edits, or intercepting them would
 * destroy the selection the user is acting on. */
bool insertsText(const QKeyEvent *event)
{
	if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
		return false;

	const QString text = event->text();

	return !text.isEmpty() && text.at(0).isPrint();
}

} // namespace

QSimpleConsole::QSimpleConsole(QWidget *parent)
    : QPlainTextEdit(parent)
{
	setUndoRedoEnabled(false); // TODO: Support undo/redo
	// Ask the platform for its fixed-width face rather than naming one: a
	// missing family falls back to a proportional font, which is the one thing
	// a console must not be.
	QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	font.setStyleHint(QFont::Monospace);
	setFont(font);

	QTextCursor cursor = textCursor();
	cursor.insertText(promptText);
	setTextCursor(cursor);
}

int32_t QSimpleConsole::inputStart() const
{
	// The promptText opens the last block. Multi-line input leaves the blocks after
	// the first without one, so fall back to the most recent block that carries
	// it rather than pointing past the end of the document.
	for (QTextBlock block = document()->lastBlock(); block.isValid();
	     block = block.previous()) {
		if (block.text().startsWith(promptText))
			return static_cast<int32_t>(block.position() + promptText.length());
	}

	return 0;
}

bool QSimpleConsole::canEditAt(const QTextCursor &cursor) const
{
	const int32_t start = inputStart();

	return cursor.selectionStart() >= start && cursor.position() >= start;
}

void QSimpleConsole::moveCursorToInputEnd()
{
	QTextCursor cursor = textCursor();
	cursor.movePosition(QTextCursor::End);

	setTextCursor(cursor);
}

void QSimpleConsole::setPrompt(const QString &prompt)
{
	const QString command = currentCommand();

	QTextCursor cursor = textCursor();
	cursor.movePosition(QTextCursor::End);
	cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
	cursor.removeSelectedText();

	promptText = prompt;

	cursor.insertText(promptText + command);

	setTextCursor(cursor);
}

QString QSimpleConsole::currentCommand() const
{
	QTextCursor cursor = textCursor();
	cursor.movePosition(QTextCursor::End);
	cursor.setPosition(inputStart(), QTextCursor::KeepAnchor);

	QString command = cursor.selectedText();
	command.replace(QChar::ParagraphSeparator, QChar::LineFeed);
	command.replace(QChar::LineSeparator, QChar::LineFeed);

	return command;
}

void QSimpleConsole::setScrollbackLines(int32_t lines)
{
	// The block limit counts the input line too, so one more than the number of
	// scrollback lines the caller asked for. Qt reads 0 as "no limit".
	setMaximumBlockCount(lines > 0 ? lines + 1 : 0);
}

int32_t QSimpleConsole::scrollbackLines() const
{
	const int32_t blocks = maximumBlockCount();

	return blocks > 0 ? blocks - 1 : 0;
}

void QSimpleConsole::setCurrentCommand(const QString &command)
{
	QTextCursor cursor = textCursor();
	cursor.movePosition(QTextCursor::End);
	cursor.setPosition(inputStart(), QTextCursor::KeepAnchor);
	cursor.insertText(command);
	cursor.movePosition(QTextCursor::End);

	setTextCursor(cursor);
}

void QSimpleConsole::handleCut(QKeyEvent *event)
{
	event->accept();

	if (!canCut())
		return;

	QTextCursor cursor = textCursor();
	QApplication::clipboard()->setText(cursor.selectedText());
	cursor.removeSelectedText();

	setTextCursor(cursor);
}

void QSimpleConsole::handleSelectAll(QKeyEvent *event)
{
	QTextCursor cursor = textCursor();
	cursor.movePosition(QTextCursor::End);
	cursor.setPosition(inputStart(), QTextCursor::KeepAnchor);

	setTextCursor(cursor);

	event->accept();
}

void QSimpleConsole::handleUpKey(QKeyEvent *event)
{
	event->accept();

	if (historyCommands.isEmpty() ||
	    historyIndex >= static_cast<int32_t>(historyCommands.size()) - 1)
		return;

	if (historyIndex < 0)
		historyDraft = currentCommand();

	historyIndex++;

	setCurrentCommand(historyCommands.at(historyIndex));
}

void QSimpleConsole::handleDownKey(QKeyEvent *event)
{
	event->accept();

	if (historyIndex < 0)
		return;

	if (historyIndex > 0) {
		historyIndex--;
		setCurrentCommand(historyCommands.at(historyIndex));
		return;
	}

	historyIndex = -1;
	setCurrentCommand(historyDraft);
	historyDraft.clear();
}

void QSimpleConsole::handleLeftRightKey(QKeyEvent *event)
{
	QTextCursor cursor = textCursor();
	int32_t offset = event->key() == Qt::Key_Left ? -1 : 1;

	if (cursor.position() + offset < inputStart()) {
		cursor.setPosition(inputStart());
		setTextCursor(cursor);
		event->accept();

		return;
	}

	QPlainTextEdit::keyPressEvent(event);
}

void QSimpleConsole::handleReturnKey(QKeyEvent *event)
{
	event->accept();

	QTextCursor cursor = textCursor();
	cursor.movePosition(QTextCursor::End);
	setTextCursor(cursor);

	const QString command = currentCommand();
	historyIndex = -1;
	historyDraft.clear();

	if (command.isEmpty()) {
		cursor.movePosition(QTextCursor::End);
		cursor.insertText("\n");
		cursor.insertText(promptText);
		setTextCursor(cursor);

		return;
	}

	addToHistory(command);

	const int32_t revision = document()->revision();

	handlingSubmittedCommand = true;
	emit commandEntered(command);
	// appendOutput clears this too, so this line only matters for a handler
	// that wrote nothing. Without it the flag would still be set when unrelated
	// output arrives later, and that output would keep the stale command line
	// instead of rebuilding it.
	handlingSubmittedCommand = false;

	// A handler that wrote no output has left the command line open. Close it,
	// so the last block is always a promptText followed by pending input.
	if (document()->revision() == revision) {
		cursor.movePosition(QTextCursor::End);
		cursor.insertText("\n");
		cursor.insertText(promptText);
		setTextCursor(cursor);
	}
}

void QSimpleConsole::handleDeleteKey(QKeyEvent *event)
{
	QTextCursor cursor = textCursor();

	if (canEditAt(cursor)) {
		QPlainTextEdit::keyPressEvent(event);
		return;
	}

	event->accept();
}

void QSimpleConsole::handleBackspaceKey(QKeyEvent *event)
{
	QTextCursor cursor = textCursor();
	const int32_t start = inputStart();

	if (cursor.hasSelection()) {
		if (canEditAt(cursor))
			QPlainTextEdit::keyPressEvent(event);
		else
			event->accept();

		return;
	}

	if (cursor.position() <= start) {
		event->accept();

		return;
	}

	// A word-wise delete can reach back past the promptText, so make the deletion
	// here rather than delegating it, and stop it at the boundary.
	if (event->matches(QKeySequence::DeleteStartOfWord)) {
		event->accept();

		cursor.movePosition(QTextCursor::PreviousWord, QTextCursor::KeepAnchor);
		if (cursor.position() < start)
			cursor.setPosition(start, QTextCursor::KeepAnchor);
		cursor.removeSelectedText();
		setTextCursor(cursor);

		return;
	}

	QPlainTextEdit::keyPressEvent(event);
}

void QSimpleConsole::handleHomeKey(QKeyEvent *event)
{
	// Ctrl+Home belongs to the scrollback - it is the only way to reach the top
	// of the document from the keyboard. Plain Home means the start of the
	// input, not the start of the visual line.
	if (event->modifiers() & Qt::ControlModifier) {
		QPlainTextEdit::keyPressEvent(event);
		return;
	}

	event->accept();

	QTextCursor cursor = textCursor();
	cursor.setPosition(inputStart(), event->modifiers() & Qt::ShiftModifier ?
	                                         QTextCursor::KeepAnchor :
	                                         QTextCursor::MoveAnchor);

	setTextCursor(cursor);
}

void QSimpleConsole::handleEndKey(QKeyEvent *event)
{
	if (event->modifiers() & Qt::ControlModifier) {
		QPlainTextEdit::keyPressEvent(event);
		return;
	}

	event->accept();

	// The counterpart to Home: the end of the input, across wraps, rather than
	// the end of the visual line.
	QTextCursor cursor = textCursor();
	cursor.movePosition(QTextCursor::End, event->modifiers() & Qt::ShiftModifier ?
	                                              QTextCursor::KeepAnchor :
	                                              QTextCursor::MoveAnchor);

	setTextCursor(cursor);
}

bool QSimpleConsole::canCut() const
{
	const QTextCursor cursor = textCursor();

	return cursor.hasSelection() && canEditAt(cursor);
}

bool QSimpleConsole::canPaste() const
{
	return !QApplication::clipboard()->text().isEmpty();
}

void QSimpleConsole::appendOutput(const QString &output)
{
	// Process output arrives newline-terminated. The prompt goes on its own
	// line below, so a terminator here would leave a blank line between every
	// pair of output lines. Exactly one is removed: "a\n\n" keeps the blank
	// line the caller asked for, and CRLF counts as one terminator.
	QString text = output;
	if (text.endsWith(QStringLiteral("\r\n")))
		text.chop(2);
	else if (text.endsWith('\n') || text.endsWith('\r'))
		text.chop(1);

	QTextCursor cursor = textCursor();
	cursor.movePosition(QTextCursor::End);

	QString command = currentCommand();

	if (!handlingSubmittedCommand) {
		// Output that arrived on its own. Take the command line apart and
		// rebuild it below the new output, so pending input follows the new
		// prompt instead of being stranded above it.
		cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
		cursor.removeSelectedText();
	} else {
		// A reply to the command still on screen. Keep that line and open the
		// next one under it.
		cursor.insertText("\n");
	}

	if (timestampEnabled) {
		// One reading for the whole call, since these lines arrived together.
		// Blank lines stay bare: a stamp on an empty line is noise.
		const QString stamp = QTime::currentTime().toString("hh:mm:ss") + ": ";
		QStringList lines = text.split('\n');
		for (QString &line : lines) {
			if (!line.isEmpty())
				line.prepend(stamp);
		}
		text = lines.join('\n');
	}

	cursor.insertText(text);
	cursor.insertText("\n");
	cursor.insertText(promptText);

	if (!handlingSubmittedCommand && !command.isEmpty()) {
		cursor.insertText(command);
	}

	// Load-bearing, not bookkeeping. A handler that writes twice sends its
	// second call down the branch above, which removes the prompt this call
	// just wrote and puts the next output line in its place - that is what
	// makes consecutive writes stack with no prompt wedged between them.
	// twoOutputsForOneCommandRenderCleanly pins the result down.
	handlingSubmittedCommand = false;

	setTextCursor(cursor);
}

void QSimpleConsole::clear()
{
	QPlainTextEdit::clear();

	QTextCursor cursor = textCursor();
	cursor.insertText(promptText);

	setTextCursor(cursor);
}

void QSimpleConsole::clearHistory()
{
	historyCommands.clear();
	historyDraft.clear();
	historyIndex = -1;
}

void QSimpleConsole::setHistory(const QStringList &commands)
{
	historyCommands = commands.mid(0, static_cast<qsizetype>(historySizeMax));

	// The replaced list may have been mid-walk, so the index and the draft now
	// describe entries that are gone.
	historyIndex = -1;
	historyDraft.clear();
}

void QSimpleConsole::setMaximumHistorySize(uint32_t commands)
{
	historySizeMax = commands;
	historyCommands = historyCommands.mid(0, static_cast<qsizetype>(historySizeMax));

	// A walk in progress may now point past the end. Clamping rather than
	// ending the walk leaves whatever the user has on screen alone; an empty
	// history lands back on -1 by itself.
	if (historyIndex >= static_cast<int32_t>(historyCommands.size()))
		historyIndex = static_cast<int32_t>(historyCommands.size()) - 1;
}

bool QSimpleConsole::saveToFile(const QString &path)
{
	QFile file(path);

	// Text mode so each line arrives with the platform's own ending.
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
		emit saveError(file.errorString());
		return false;
	}

	QTextStream stream(&file);
	stream << toPlainText() << '\n';
	stream.flush();

	// The destructor would flush but could not report a failure, and a short
	// write on a full disk surfaces nowhere else.
	if (stream.status() != QTextStream::Ok || !file.flush()) {
		emit saveError(file.errorString());
		return false;
	}

	return true;
}

void QSimpleConsole::saveAs()
{
	const QString path = QFileDialog::getSaveFileName(this, tr("Save Console"),
	                                                  QStringLiteral("console.txt"),
	                                                  tr("Text files (*.txt);;All files (*)"));
	if (!path.isEmpty())
		saveToFile(path);
}

void QSimpleConsole::keyPressEvent(QKeyEvent *event)
{
	// Before Cut, and before Qt's own Copy: a terminal reads Ctrl+C as an
	// interrupt when there is nothing to copy.
	if (event->matches(QKeySequence::Copy) && !textCursor().hasSelection()) {
		event->accept();
		emit interruptRequested();
		return;
	}

	if (event->matches(QKeySequence::Cut)) {
		handleCut(event);
		return;
	}

	if (event->matches(QKeySequence::SelectAll)) {
		handleSelectAll(event);
		return;
	}

	switch (event->key()) {
	case Qt::Key_Up:
		handleUpKey(event);
		return;
	case Qt::Key_Down:
		handleDownKey(event);
		return;
	case Qt::Key_Left:
	case Qt::Key_Right:
		handleLeftRightKey(event);
		return;
	case Qt::Key_Enter:
	case Qt::Key_Return:
		handleReturnKey(event);
		return;
	case Qt::Key_Delete:
		handleDeleteKey(event);
		return;
	case Qt::Key_Backspace:
		handleBackspaceKey(event);
		return;
	case Qt::Key_Tab:
		// A literal tab in a command line is no use to anyone. Hand the input
		// to whoever can complete it instead - but only for a bare Tab, since
		// Ctrl+Tab belongs to the host application, which commonly uses it to
		// switch views.
		if (event->modifiers() != Qt::NoModifier)
			break;
		event->accept();
		emit completionRequested(currentCommand());
		return;
	case Qt::Key_Home:
		handleHomeKey(event);
		return;
	case Qt::Key_End:
		handleEndKey(event);
		return;
	default:
		break;
	}

	// Anything that would insert text has to land after the promptText. Typing
	// outside the editable region moves to the input instead of editing the
	// scrollback, which is what a terminal does.
	if (insertsText(event) && !canEditAt(textCursor()))
		moveCursorToInputEnd();

	QPlainTextEdit::keyPressEvent(event);
}

void QSimpleConsole::contextMenuEvent(QContextMenuEvent *event)
{
	std::unique_ptr<QMenu> menu(createStandardContextMenu());
	QAction *action;

	if ((action = menu->findChild<QAction *>("edit-cut")))
		action->setEnabled(canCut());
	if ((action = menu->findChild<QAction *>("edit-delete")))
		action->setEnabled(canCut());
	if ((action = menu->findChild<QAction *>("edit-paste")))
		action->setEnabled(canPaste());

	menu->addSeparator();
	QAction *saveAction = menu->addAction(tr("Save As..."));
	QAction *clearAction = menu->addAction(tr("Clear"));

	// Dispatching on the return value rather than connecting the actions keeps
	// the file dialog out of the menu's own event loop, where it would open
	// while the popup still holds the mouse grab.
	const QAction *chosen = menu->exec(event->globalPos());

	if (chosen == saveAction)
		saveAs();
	else if (chosen == clearAction)
		clear();
}

/* Every insertion the user can make - Ctrl+V, Shift+Insert, middle-click,
 * drag and drop, the context menu - arrives here, so this is the one place
 * that has to keep text inside the input line. */
void QSimpleConsole::insertFromMimeData(const QMimeData *source)
{
	QString text = source->text();
	if (text.isEmpty())
		return;

	if (!canEditAt(textCursor()))
		moveCursorToInputEnd();

	while (text.endsWith('\n') || text.endsWith('\r'))
		text.chop(1);

	// The promptText opens a single block, so a pasted line break would leave a
	// line with no promptText on it. Fold the breaks into spaces instead, which
	// keeps every character of the paste visible and editable.
	text.replace(QRegularExpression(QStringLiteral("[\\r\\n\\x{2028}\\x{2029}]+")),
	             QStringLiteral(" "));

	textCursor().insertText(text);
}

void QSimpleConsole::addToHistory(const QString &command)
{
	if (!historyCommands.isEmpty() && historyCommands.first() == command)
		return;

	historyCommands.prepend(command);
	if (historyCommands.size() > static_cast<qsizetype>(historySizeMax))
		historyCommands.removeLast();
}
