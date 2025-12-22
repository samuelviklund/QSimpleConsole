#include "qsimpleconsole.h"

#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QPalette>

QSimpleConsole::QSimpleConsole()
{
	setUndoRedoEnabled(false); // TODO: Support undo/redo
	setFont(QFont("Source Code Pro"));

	QTextCursor cursor = textCursor();
	cursor.insertText(prompt);
	setTextCursor(cursor);

	promptEndPosition = cursor.position();
}

void QSimpleConsole::setPrompt(const QString &p)
{
	prompt = p;

	QTextCursor cursor = textCursor();
	promptEndPosition = cursor.position();
}

QString QSimpleConsole::getCommand()
{
	QTextCursor cursor = textCursor();
	QString command;

	cursor.movePosition(QTextCursor::End);
	cursor.setPosition(promptEndPosition, QTextCursor::KeepAnchor);
	command = cursor.selectedText();
	command.replace(QChar::LineSeparator, QChar::LineFeed);

	setTextCursor(cursor);

	return command;
}

void QSimpleConsole::setCurrentCommand(const QString &command)
{
	QTextCursor cursor = textCursor();
	cursor.movePosition(QTextCursor::End);
	cursor.setPosition(promptEndPosition, QTextCursor::KeepAnchor);
	cursor.insertText(command);
	cursor.movePosition(QTextCursor::End);

	setTextCursor(cursor);
}

void QSimpleConsole::handleCut(QKeyEvent *event)
{
	event->accept();

	QTextCursor cursor = textCursor();
	if (cursor.selectionStart() < promptEndPosition)
		return;

	QApplication::clipboard()->setText(cursor.selectedText());
	cursor.removeSelectedText();

	setTextCursor(cursor);
}

void QSimpleConsole::handlePaste(QKeyEvent *event)
{
	QTextCursor cursor = textCursor();

	if (cursor.position() < promptEndPosition) {
		event->accept();

		return;
	}

	QPlainTextEdit::keyPressEvent(event);
}

void QSimpleConsole::handleSelectAll(QKeyEvent *event)
{
	QTextCursor cursor = textCursor();
	cursor.movePosition(QTextCursor::End);
	cursor.setPosition(promptEndPosition, QTextCursor::KeepAnchor);

	setTextCursor(cursor);

	event->accept();
}

void QSimpleConsole::handleUpKey(QKeyEvent *event)
{
	event->accept();

	if (history.empty() || historyIndex >= (int32_t) history.size() - 1)
		return;

	historyIndex++;

	setCurrentCommand(history.at(historyIndex));

}

void QSimpleConsole::handleDownKey(QKeyEvent *event)
{
	event->accept();

	if (history.empty())
		return;

	if (historyIndex > 0) {
		historyIndex--;
		setCurrentCommand(history.at(historyIndex));
		return;
	}

	historyIndex = -1;
	setCurrentCommand("");
}

void QSimpleConsole::handleLeftRightKey(QKeyEvent *event)
{
	QTextCursor cursor = textCursor();
	int8_t offset = event->key() == Qt::Key_Left ? -1 : 1;

	if (cursor.position() + offset < promptEndPosition) {
		cursor.setPosition(promptEndPosition);
		setTextCursor(cursor);
		event->accept();

		return;
	}

	QPlainTextEdit::keyPressEvent(event);
}

void QSimpleConsole::handleReturnKey(QKeyEvent *event)
{
	QTextCursor cursor = textCursor();
	cursor.movePosition(QTextCursor::End);
	setTextCursor(cursor);

	QString command = getCommand();
	if (!command.isEmpty()) {
		pushHistoryCommand(command);
		isUserInput = true;
		emit commandEntered(command);
	} else {
		cursor.insertText("\n");
		cursor.insertText(prompt);
	}
	promptEndPosition = cursor.position();
	historyIndex = -1;

	event->accept();
}

void QSimpleConsole::handleDeleteKey(QKeyEvent *event)
{
	QTextCursor cursor = textCursor();

	if ((cursor.selectedText().length() == 0 && cursor.position() >= promptEndPosition) ||
	    (cursor.selectedText().length() > 0 && cursor.selectionStart() > (promptEndPosition - 1))) {
		QPlainTextEdit::keyPressEvent(event);
		return;
	}

	event->accept();
}

void QSimpleConsole::handleBackspaceKey(QKeyEvent *event)
{
	QTextCursor cursor = textCursor();

	if ((cursor.selectedText().length() == 0 && cursor.position() > promptEndPosition) ||
	    (cursor.selectedText().length() > 0 && cursor.selectionStart() > (promptEndPosition - 1))) {
		QPlainTextEdit::keyPressEvent(event);
		return;
	}

	event->accept();
}

void QSimpleConsole::handleHomeKey(QKeyEvent *event)
{
	QTextCursor cursor = textCursor();
	cursor.setPosition(promptEndPosition);
	setTextCursor(cursor);

	event->accept();
}

bool QSimpleConsole::canCut() const
{
	QTextCursor cursor = textCursor();

	if (!cursor.hasSelection())
		return false;

	if (cursor.selectionStart() < promptEndPosition)
		return false;

	return true;
}

bool QSimpleConsole::canPaste() const
{
	return textCursor().position() >= promptEndPosition;
}

void QSimpleConsole::appendOutput(const QString &result)
{
	QTextCursor cursor = textCursor();
	cursor.movePosition(QTextCursor::End);

	QString command = getCommand();
	qDebug() << command;

	if (!isUserInput) {
		cursor.select(QTextCursor::LineUnderCursor);
		cursor.removeSelectedText();
	} else {
		cursor.insertText("\n");
	}

	if (useTimestamp) {
		cursor.insertText(QTime::currentTime().toString("hh:mm:ss") + ": ");
	}

	cursor.insertText(result);
	cursor.insertText("\n");
	cursor.insertText(prompt);

	promptEndPosition = cursor.position();

	if (!isUserInput && !command.isEmpty()) {
		cursor.insertText(command);
		//setCurrentCommand(command);
	}

	if (isUserInput)
		isUserInput = false;

	setTextCursor(cursor);
}

void QSimpleConsole::keyPressEvent(QKeyEvent *event)
{
	int key = event->key();
	Qt::KeyboardModifiers modifiers = event->modifiers();

	/*
	if (modifiers == Qt::ControlModifier && key == Qt::Key_C) {
		// Copy
		//return;
	}
	*/

	if (modifiers == Qt::ControlModifier && key == Qt::Key_V) {
		handlePaste(event);
		return;
	}

	if (modifiers == Qt::ControlModifier && key == Qt::Key_X) {
		handleCut(event);
		return;
	}

	if (modifiers == Qt::ControlModifier && key == Qt::Key_A) {
		handleSelectAll(event);
		return;
	}

	if (modifiers == Qt::ShiftModifier) {
		QPlainTextEdit::keyPressEvent(event);
		return;
	}

	switch (key) {
	case Qt::Key_Up:
		handleUpKey(event);
		break;
	case Qt::Key_Down:
		handleDownKey(event);
		break;
	case Qt::Key_Left:
	case Qt::Key_Right:
		handleLeftRightKey(event);
		break;
	case Qt::Key_Enter:
	case Qt::Key_Return:
		handleReturnKey(event);
		break;
	case Qt::Key_Delete:
		handleDeleteKey(event);
		break;
	case Qt::Key_Backspace:
		handleBackspaceKey(event);
		break;
	case Qt::Key_Home:
		handleHomeKey(event);
		break;
	default:
		QPlainTextEdit::keyPressEvent(event);
	}
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

void QSimpleConsole::pushHistoryCommand(const QString &command)
{
	history.push_front(command);
	if (history.size() > historySizeMax)
		history.pop_back();
}
