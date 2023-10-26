#ifndef QSIMPLECONSOLE_H
#define QSIMPLECONSOLE_H

#include <QPlainTextEdit>

#include <deque>

class QSimpleConsole : public QPlainTextEdit
{
    Q_OBJECT

public:
    QSimpleConsole();

    void setLineHeader(QString header);
    QString getCommand();
    void replaceCommand(QString command);

    void processCut(QKeyEvent *event);
    void processPaste(QKeyEvent *event);
    void processSelectAll(QKeyEvent *event);
    void processUpKey(QKeyEvent *event);
    void processDownKey(QKeyEvent *event);
    void processLeftRightKey(QKeyEvent *event);
    void processReturnKey(QKeyEvent *event);
    void processDeleteKey(QKeyEvent *event);
    void processBackspaceKey(QKeyEvent *event);
    void processHomeKey(QKeyEvent *event);

    bool isCuttable();
    bool isPasteable();

public slots:
    void writeOutput(QString result);

signals:
    void consoleCommand(QString command);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void pushHistoryCommand(QString command);

    QString lineHeader;
    int32_t inputStartPosition;
    std::deque<QString> history;
    uint32_t historySizeMax = 32;
    int32_t historyIndex = -1;
};

#endif // QSIMPLECONSOLE_H
