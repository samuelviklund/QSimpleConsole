#include "qsimpleconsole.h"

#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QPalette>

#include <iostream>

QSimpleConsole::QSimpleConsole()
{
    setUndoRedoEnabled(false); // TODO: Support undo/redo
    qDebug() << textInteractionFlags();
    setFont(QFont("Source Code Pro"));
    setLineHeader("> ");
    textCursor().insertText(lineHeader);
}

void QSimpleConsole::setLineHeader(QString header)
{
    lineHeader = header;
    inputStartPosition = header.length();
}

QString QSimpleConsole::getCommand()
{
    QTextCursor textCursor = this->textCursor();
    QString command;

    textCursor.movePosition(QTextCursor::End);
    textCursor.setPosition(inputStartPosition, QTextCursor::KeepAnchor);
    command = textCursor.selectedText();
    command.replace(QChar::LineSeparator, QChar::LineFeed);

    return command;
}

void QSimpleConsole::replaceCommand(QString command)
{
    QTextCursor textCursor = this->textCursor();
    textCursor.movePosition(QTextCursor::End);
    textCursor.setPosition(inputStartPosition, QTextCursor::KeepAnchor);
    textCursor.insertText(command);
    textCursor.movePosition(QTextCursor::End);

    setTextCursor(textCursor);
}

void QSimpleConsole::processCut(QKeyEvent *event)
{
    event->accept();

    QTextCursor textCursor = this->textCursor();
    if (textCursor.selectionStart() < inputStartPosition)
        return;

    QApplication::clipboard()->setText(textCursor.selectedText());
    textCursor.removeSelectedText();

    setTextCursor(textCursor);
}

void QSimpleConsole::processPaste(QKeyEvent *event)
{
    QTextCursor textCursor = this->textCursor();

    if (textCursor.position() < inputStartPosition) {
        event->accept();

        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

void QSimpleConsole::processSelectAll(QKeyEvent *event)
{
    QTextCursor textCursor = this->textCursor();
    textCursor.movePosition(QTextCursor::End);
    textCursor.setPosition(inputStartPosition, QTextCursor::KeepAnchor);

    setTextCursor(textCursor);

    event->accept();
}

void QSimpleConsole::processUpKey(QKeyEvent *event)
{
    event->accept();

    if (history.size() == 0 || historyIndex >= (int32_t) history.size() - 1)
        return;

    historyIndex++;

    replaceCommand(history.at(historyIndex));

}

void QSimpleConsole::processDownKey(QKeyEvent *event)
{
    event->accept();

    if (history.size() == 0)
        return;

    if (historyIndex > 0) {
        historyIndex--;
        qDebug() << history.at(historyIndex) << ", " << historyIndex;
        replaceCommand(history.at(historyIndex));
        return;
    }

    historyIndex = -1;
    replaceCommand("");
}

void QSimpleConsole::processLeftRightKey(QKeyEvent *event)
{
    QTextCursor textCursor = this->textCursor();
    int8_t direction = event->key() == Qt::Key_Left ? -1 : 1;

    if (textCursor.position() + direction < inputStartPosition) {
        textCursor.setPosition(inputStartPosition);
        setTextCursor(textCursor);
        event->accept();

        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

void QSimpleConsole::processReturnKey(QKeyEvent *event)
{
    QTextCursor textCursor = this->textCursor();
    textCursor.movePosition(QTextCursor::End);
    setTextCursor(textCursor);

    QString command = getCommand();
    if (!command.isEmpty()) {
        pushHistoryCommand(command);
        emit consoleCommand(command);
    } else {
        textCursor.insertText("\n");
        textCursor.insertText(lineHeader);
    }
    inputStartPosition = textCursor.position();
    historyIndex = -1;

    event->accept();
}

void QSimpleConsole::processDeleteKey(QKeyEvent *event)
{
    QTextCursor textCursor = this->textCursor();

    if ((textCursor.selectedText().length() == 0 && textCursor.position() >= inputStartPosition) ||
        (textCursor.selectedText().length() > 0 && textCursor.selectionStart() > (inputStartPosition - 1))) {
        QPlainTextEdit::keyPressEvent(event);
        return;
    }

    event->accept();
}

void QSimpleConsole::processBackspaceKey(QKeyEvent *event)
{
    QTextCursor textCursor = this->textCursor();

    if ((textCursor.selectedText().length() == 0 && textCursor.position() > inputStartPosition) ||
        (textCursor.selectedText().length() > 0 && textCursor.selectionStart() > (inputStartPosition - 1))) {
        QPlainTextEdit::keyPressEvent(event);
        return;
    }

    event->accept();
}

void QSimpleConsole::processHomeKey(QKeyEvent *event)
{
    QTextCursor textCursor = this->textCursor();
    textCursor.setPosition(inputStartPosition);
    setTextCursor(textCursor);

    event->accept();
}

bool QSimpleConsole::isCuttable()
{
    QTextCursor textCursor = this->textCursor();

    if (!textCursor.hasSelection())
        return false;

    if (textCursor.selectionStart() < inputStartPosition)
        return false;

    return true;
}

bool QSimpleConsole::isPasteable()
{
    return textCursor().position() >= inputStartPosition;
}

void QSimpleConsole::writeOutput(QString result)
{
    QTextCursor textCursor = this->textCursor();
    textCursor.movePosition(QTextCursor::End);
    setTextCursor(textCursor);

    textCursor.insertText("\n");
    textCursor.insertText(result);
    textCursor.insertText("\n");
    textCursor.insertText(lineHeader);
    inputStartPosition = textCursor.position();
}

void QSimpleConsole::keyPressEvent(QKeyEvent *event)
{
    int key = event->key();
    Qt::KeyboardModifiers modifiers = event->modifiers();

    if (modifiers == Qt::ControlModifier && key == Qt::Key_C) {
        // Copy
        //return;
    }

    if (modifiers == Qt::ControlModifier && key == Qt::Key_V) {
        processPaste(event);
        return;
    }

    if (modifiers == Qt::ControlModifier && key == Qt::Key_X) {
        processCut(event);
        return;
    }

    if (modifiers == Qt::ControlModifier && key == Qt::Key_A) {
        processSelectAll(event);
        return;
    }

    if (modifiers == Qt::ShiftModifier) {
        QPlainTextEdit::keyPressEvent(event);
        return;
    }

    switch (key) {
    case Qt::Key_Up:
        processUpKey(event);
        break;
    case Qt::Key_Down:
        processDownKey(event);
        break;
    case Qt::Key_Left:
    case Qt::Key_Right:
        processLeftRightKey(event);
        break;
    case Qt::Key_Enter:
    case Qt::Key_Return:
        processReturnKey(event);
        break;
    case Qt::Key_Delete:
        processDeleteKey(event);
        break;
    case Qt::Key_Backspace:
        processBackspaceKey(event);
        break;
    case Qt::Key_Home:
        processHomeKey(event);
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
        action->setEnabled(isCuttable());
    if ((action = menu->findChild<QAction*>("edit-delete")))
        action->setEnabled(isCuttable());
    if ((action = menu->findChild<QAction*>("edit-paste")))
        action->setEnabled(isPasteable());

    menu->exec(event->globalPos());
    delete menu;
}

void QSimpleConsole::pushHistoryCommand(QString command)
{
    history.push_front(command);
    if (history.size() > historySizeMax)
        history.pop_back();
}
