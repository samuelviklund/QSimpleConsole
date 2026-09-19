#include "qsimpleconsole.h"

#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QMimeData>
#include <QPalette>
#include <QRegularExpression>
#include <QTextBlock>

namespace {

/* True for the keys that would put text into the document. Copy, undo and the
 * navigation keys must not be treated as edits, or intercepting them would
 * destroy the selection the user is acting on. */
bool insertsText(const QKeyEvent *event)
{
	if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
		return false;

	if (event->key() == Qt::Key_Tab)
		return true;

	const QString text = event->text();

	return !text.isEmpty() && text.at(0).isPrint();
}

} // namespace

QSimpleConsole::QSimpleConsole(QWidget *parent)
    : QPlainTextEdit(parent)
{
	setUndoRedoEnabled(false); // TODO: Support undo/redo
	setFont(QFont("Source Code Pro"));

	QTextCursor cursor = textCursor();
	cursor.insertText(prompt);
	setTextCursor(cursor);
}

int QSimpleConsole::inputStart() const
{
	// The prompt opens the last block. Multi-line input leaves the blocks after
	// the first without one, so fall back to the most recent block that carries
	// it rather than pointing past the end of the document.
	for (QTextBlock block = document()->lastBlock(); block.isValid(); block = block.previous()) {
		if (block.text().startsWith(prompt))
			return block.position() + prompt.length();
	}

	return 0;
}

bool QSimpleConsole::canEditAt(const QTextCursor &cursor) const
{
	const int start = inputStart();

	return cursor.selectionStart() >= start && cursor.position() >= start;
}

void QSimpleConsole::moveCursorToInputEnd()
{
	QTextCursor cursor = textCursor();
	cursor.movePosition(QTextCursor::End);

	setTextCursor(cursor);
}

void QSimpleConsole::setPrompt(const QString &p)
{
	const QString command = getCommand();

	QTextCursor cursor = textCursor();
	cursor.movePosition(QTextCursor::End);
	cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
	cursor.removeSelectedText();

	prompt = p;

	cursor.insertText(prompt + command);

	setTextCursor(cursor);
}

QString QSimpleConsole::getCommand()
{
	QTextCursor cursor = textCursor();
	QString command;

	cursor.movePosition(QTextCursor::End);
	cursor.setPosition(inputStart(), QTextCursor::KeepAnchor);
	command = cursor.selectedText();
	command.replace(QChar::ParagraphSeparator, QChar::LineFeed);
	command.replace(QChar::LineSeparator, QChar::LineFeed);

	setTextCursor(cursor);

	return command;
}

void QSimpleConsole::setScrollbackLines(int lines)
{
	// The block limit counts the input line too, so one more than the number of
	// scrollback lines the caller asked for. Qt reads 0 as "no limit".
	setMaximumBlockCount(lines > 0 ? lines + 1 : 0);
}

int QSimpleConsole::scrollbackLines() const
{
	const int blocks = maximumBlockCount();

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

	if (history.empty() || historyIndex >= (int32_t) history.size() - 1)
		return;

	if (historyIndex < 0)
		historyDraft = getCommand();

	historyIndex++;

	setCurrentCommand(history.at(historyIndex));
}

void QSimpleConsole::handleDownKey(QKeyEvent *event)
{
	event->accept();

	if (historyIndex < 0)
		return;

	if (historyIndex > 0) {
		historyIndex--;
		setCurrentCommand(history.at(historyIndex));
		return;
	}

	historyIndex = -1;
	setCurrentCommand(historyDraft);
	historyDraft.clear();
}

void QSimpleConsole::handleLeftRightKey(QKeyEvent *event)
{
	QTextCursor cursor = textCursor();
	int8_t offset = event->key() == Qt::Key_Left ? -1 : 1;

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

	const QString command = getCommand();
	historyIndex = -1;
	historyDraft.clear();

	if (command.isEmpty()) {
		cursor.movePosition(QTextCursor::End);
		cursor.insertText("\n");
		cursor.insertText(prompt);
		setTextCursor(cursor);

		return;
	}

	addToHistory(command);

	const int revision = document()->revision();

	handlingSubmittedCommand = true;
	emit commandEntered(command);
	handlingSubmittedCommand = false;

	// A handler that wrote no output has left the command line open. Close it,
	// so the last block is always a prompt followed by pending input.
	if (document()->revision() == revision) {
		cursor.movePosition(QTextCursor::End);
		cursor.insertText("\n");
		cursor.insertText(prompt);
		setTextCursor(cursor);
	}
}

void QSimpleConsole::handleDeleteKey(QKeyEvent *event)
{
	QTextCursor cursor = textCursor();
	const int start = inputStart();

	if (cursor.hasSelection() ? cursor.selectionStart() >= start : cursor.position() >= start) {
		QPlainTextEdit::keyPressEvent(event);
		return;
	}

	event->accept();
}

void QSimpleConsole::handleBackspaceKey(QKeyEvent *event)
{
	QTextCursor cursor = textCursor();
	const int start = inputStart();

	if (cursor.hasSelection()) {
		if (cursor.selectionStart() >= start)
			QPlainTextEdit::keyPressEvent(event);
		else
			event->accept();

		return;
	}

	if (cursor.position() <= start) {
		event->accept();

		return;
	}

	// A word-wise delete can reach back past the prompt, so make the deletion
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
	event->accept();

	QTextCursor cursor = textCursor();
	cursor.setPosition(inputStart(), event->modifiers() & Qt::ShiftModifier
	                                    ? QTextCursor::KeepAnchor
	                                    : QTextCursor::MoveAnchor);

	setTextCursor(cursor);
}

bool QSimpleConsole::canCut() const
{
	QTextCursor cursor = textCursor();

	if (!cursor.hasSelection())
		return false;

	if (cursor.selectionStart() < inputStart())
		return false;

	return true;
}

bool QSimpleConsole::canPaste() const
{
	return !QApplication::clipboard()->text().isEmpty();
}

void QSimpleConsole::appendOutput(const QString &output)
{
	QTextCursor cursor = textCursor();
	cursor.movePosition(QTextCursor::End);

	QString command = getCommand();

	if (!handlingSubmittedCommand) {
		cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
		cursor.removeSelectedText();
	} else {
		cursor.insertText("\n");
	}

	if (useTimestamp) {
		cursor.insertText(QTime::currentTime().toString("hh:mm:ss") + ": ");
	}

	cursor.insertText(output);
	cursor.insertText("\n");
	cursor.insertText(prompt);

	if (!handlingSubmittedCommand && !command.isEmpty()) {
		cursor.insertText(command);
	}

	handlingSubmittedCommand = false;

	setTextCursor(cursor);
}

void QSimpleConsole::keyPressEvent(QKeyEvent *event)
{
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
	case Qt::Key_Home:
		handleHomeKey(event);
		return;
	default:
		break;
	}

	// Anything that would insert text has to land after the prompt. Typing
	// outside the editable region moves to the input instead of editing the
	// scrollback, which is what a terminal does.
	if (insertsText(event) && !canEditAt(textCursor()))
		moveCursorToInputEnd();

	QPlainTextEdit::keyPressEvent(event);
}

void QSimpleConsole::contextMenuEvent(QContextMenuEvent *event)
{
	QMenu *menu = createStandardContextMenu();
	QAction *action;

	if ((action = menu->findChild<QAction*>("edit-cut")))
		action->setEnabled(canCut());
	if ((action = menu->findChild<QAction*>("edit-delete")))
		action->setEnabled(canCut());
	if ((action = menu->findChild<QAction*>("edit-paste")))
		action->setEnabled(canPaste());

	menu->exec(event->globalPos());
	delete menu;
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

	// The prompt opens a single block, so a pasted line break would leave a
	// line with no prompt on it. Fold the breaks into spaces instead, which
	// keeps every character of the paste visible and editable.
	text.replace(QRegularExpression(QStringLiteral("[\\r\\n\\x{2028}\\x{2029}]+")),
	             QStringLiteral(" "));

	textCursor().insertText(text);
}

void QSimpleConsole::addToHistory(const QString &command)
{
	if (!history.empty() && history.front() == command)
		return;

	history.push_front(command);
	if (history.size() > historySizeMax)
		history.pop_back();
}
