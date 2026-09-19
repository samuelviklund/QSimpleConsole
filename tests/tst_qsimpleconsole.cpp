#include <QtTest>

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QSignalSpy>
#include <QTextBlock>

#include "qsimpleconsole.h"

/*
 * Two kinds of test live here.
 *
 *  - Regression guards: behaviour that was wrong and has been fixed. Each
 *    carries the finding id from FINDINGS.html. They were written first, as
 *    QEXPECT_FAIL, so that landing a fix turned the suite red until the
 *    marker came out - no fix was claimed without the evidence.
 *
 *  - Characterization: behaviour that was already correct and had to survive
 *    the prompt-boundary rework.
 */
class tst_QSimpleConsole : public QObject
{
	Q_OBJECT

private slots:
	// -- characterization: the prompt boundary ---------------------------
	void promptIsDrawnOnConstruction();
	void typedTextIsEmittedAsCommand();
	void emptyReturnDrawsNewPrompt();
	void homeMovesToInputStart();
	void backspaceCannotDeletePrompt();
	void deleteWithFullSelectionIsRefused();
	void backspaceWithFullSelectionIsRefused();
	void ctrlLeftCannotCrossPromptBoundary();
	void selectAllSelectsOnlyTheCommand();

	// -- characterization: output ----------------------------------------
	void asyncOutputPreservesPendingInput();
	void twoOutputsForOneCommandRenderCleanly();
	void timestampPrefixesOutput();
	void maximumBlockCountDoesNotBreakBoundary();
	void scrollbackIsUnlimitedByDefault();
	void scrollbackLinesCapsHistory();
	void scrollbackCapPreservesPendingInput();
	void clearRedrawsThePrompt();
	void clearLeavesTheConsoleUsable();
	void clearKeepsHistory();
	void clearHistoryDropsRecordedCommands();

	// -- characterization: history ---------------------------------------
	void upDownNavigatesHistory();
	void historyIsCappedAtMaximum();

	// -- regression guards: were failing, now fixed ------------------------
	void mouseSelectAllThenTypeKeepsPrompt();           // F01
	void typingInsideOutputAppendsToInput();            // F01
	void commandStillEmittedAfterMouseSelectAll();      // F01
	void setPromptRedrawsPrompt();                      // F02
	void setPromptPreservesPendingCommand();            // F02
	void handlerMayPrefillNextCommand();                // F03
	void multiLineCommandUsesNewlines();                // F04
	void shiftReturnDoesNotInsertRawNewline();          // F05
	void shiftBackspaceCannotDeletePrompt();            // F05
	void shiftHomeThenTypeKeepsPrompt();                // F05
	void shiftInsertRespectsPromptBoundary();           // F06
	void asyncOutputPreservesWrappedInput();            // F07
	void silentHandlerPreservesPendingInput();          // F08
	void ctrlBackspaceCannotDeleteAlphanumericPrompt(); // F09
	void cutWithoutSelectionKeepsClipboard();           // F10
	void pastedNewlinesStayOnOneLine();                 // F11
	void droppedTextLandsInInput();                     // F12
	void historyRestoresDraftLine();                    // F21
	void historySkipsConsecutiveDuplicates();           // F21
};

namespace {

/* Echoes every command back, which is what the README's example does and
 * what most consumers will do. */
void echoCommands(QSimpleConsole *console)
{
	QObject::connect(console, &QSimpleConsole::commandEntered, console,
	                 &QSimpleConsole::appendOutput);
}

void enter(QSimpleConsole *console, const QString &command)
{
	QTest::keyClicks(console, command);
	QTest::keyClick(console, Qt::Key_Return);
}

/* insertFromMimeData is protected, and the drop and middle-click paths reach
 * it without going through a key event, so a test needs the same access. */
class PastableConsole : public QSimpleConsole
{
public:
	using QSimpleConsole::insertFromMimeData;
};

} // namespace

// ---------------------------------------------------------------------------
// characterization: the prompt boundary
// ---------------------------------------------------------------------------

void tst_QSimpleConsole::promptIsDrawnOnConstruction()
{
	QSimpleConsole console;
	console.show();

	QCOMPARE(console.toPlainText(), QStringLiteral("> "));
	QCOMPARE(console.getCommand(), QString());
}

void tst_QSimpleConsole::typedTextIsEmittedAsCommand()
{
	QSimpleConsole console;
	console.show();
	QSignalSpy spy(&console, &QSimpleConsole::commandEntered);

	enter(&console, "hello");

	QCOMPARE(spy.count(), 1);
	QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("hello"));
}

void tst_QSimpleConsole::emptyReturnDrawsNewPrompt()
{
	QSimpleConsole console;
	console.show();
	QSignalSpy spy(&console, &QSimpleConsole::commandEntered);

	QTest::keyClick(&console, Qt::Key_Return);

	QCOMPARE(spy.count(), 0);
	QCOMPARE(console.toPlainText(), QStringLiteral("> \n> "));
}

void tst_QSimpleConsole::homeMovesToInputStart()
{
	QSimpleConsole console;
	console.show();
	QTest::keyClicks(&console, "abc");

	QTest::keyClick(&console, Qt::Key_Home);

	QCOMPARE(console.textCursor().position(), 2); // just past "> "
}

void tst_QSimpleConsole::backspaceCannotDeletePrompt()
{
	QSimpleConsole console;
	console.show();

	for (int i = 0; i < 4; ++i)
		QTest::keyClick(&console, Qt::Key_Backspace);

	QCOMPARE(console.toPlainText(), QStringLiteral("> "));
}

void tst_QSimpleConsole::deleteWithFullSelectionIsRefused()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "hello");
	const QString before = console.toPlainText();

	QTextCursor cursor = console.textCursor();
	cursor.setPosition(0);
	cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
	console.setTextCursor(cursor);
	QTest::keyClick(&console, Qt::Key_Delete);

	QCOMPARE(console.toPlainText(), before);
}

void tst_QSimpleConsole::backspaceWithFullSelectionIsRefused()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "hello");
	const QString before = console.toPlainText();

	QTextCursor cursor = console.textCursor();
	cursor.setPosition(0);
	cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
	console.setTextCursor(cursor);
	QTest::keyClick(&console, Qt::Key_Backspace);

	QCOMPARE(console.toPlainText(), before);
}

void tst_QSimpleConsole::ctrlLeftCannotCrossPromptBoundary()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "one two");
	QTest::keyClicks(&console, "three");

	for (int i = 0; i < 6; ++i)
		QTest::keyClick(&console, Qt::Key_Left, Qt::ControlModifier);

	const int inputStart = console.document()->lastBlock().position() + 2;
	QCOMPARE(console.textCursor().position(), inputStart);
}

void tst_QSimpleConsole::selectAllSelectsOnlyTheCommand()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "hello");
	QTest::keyClicks(&console, "abc");

	QTest::keyClick(&console, Qt::Key_A, Qt::ControlModifier);

	QCOMPARE(console.textCursor().selectedText(), QStringLiteral("abc"));
}

// ---------------------------------------------------------------------------
// characterization: output
// ---------------------------------------------------------------------------

void tst_QSimpleConsole::asyncOutputPreservesPendingInput()
{
	QSimpleConsole console;
	console.show();
	QTest::keyClicks(&console, "typing");

	console.appendOutput("ASYNC");

	QCOMPARE(console.toPlainText(), QStringLiteral("ASYNC\n> typing"));
	QCOMPARE(console.getCommand(), QStringLiteral("typing"));
}

void tst_QSimpleConsole::twoOutputsForOneCommandRenderCleanly()
{
	QSimpleConsole console;
	console.show();
	connect(&console, &QSimpleConsole::commandEntered, &console, [&console](const QString &) {
		console.appendOutput("line1");
		console.appendOutput("line2");
	});

	enter(&console, "cmd");

	QCOMPARE(console.toPlainText(), QStringLiteral("> cmd\nline1\nline2\n> "));
}

void tst_QSimpleConsole::timestampPrefixesOutput()
{
	QSimpleConsole console;
	console.show();
	console.setTimestamp(true);

	console.appendOutput("hello");

	const QStringList lines = console.toPlainText().split('\n');
	QVERIFY(lines.size() >= 2);
	QVERIFY2(QRegularExpression("^\\d{2}:\\d{2}:\\d{2}: hello$").match(lines.at(0)).hasMatch(),
	         qPrintable("unexpected output line: " + lines.at(0)));
}

void tst_QSimpleConsole::maximumBlockCountDoesNotBreakBoundary()
{
	QSimpleConsole console;
	console.show();
	console.document()->setMaximumBlockCount(3);
	echoCommands(&console);

	for (const QString &command : { QStringLiteral("one"), QStringLiteral("two"),
	                                QStringLiteral("three"), QStringLiteral("four") })
		enter(&console, command);
	QTest::keyClicks(&console, "next");

	QCOMPARE(console.getCommand(), QStringLiteral("next"));
}

// ---------------------------------------------------------------------------
// characterization: history
// ---------------------------------------------------------------------------

void tst_QSimpleConsole::scrollbackIsUnlimitedByDefault()
{
	QSimpleConsole console;
	console.show();

	QCOMPARE(console.scrollbackLines(), 0);

	for (int i = 0; i < 200; ++i)
		console.appendOutput(QStringLiteral("line %1").arg(i));

	QCOMPARE(console.document()->blockCount(), 201);
}

void tst_QSimpleConsole::scrollbackLinesCapsHistory()
{
	QSimpleConsole console;
	console.show();
	console.setScrollbackLines(5);

	QCOMPARE(console.scrollbackLines(), 5);

	for (int i = 0; i < 50; ++i)
		console.appendOutput(QStringLiteral("line %1").arg(i));

	// five output lines, plus the input line the limit also counts
	QCOMPARE(console.document()->blockCount(), 6);
	QVERIFY2(console.toPlainText().startsWith(QStringLiteral("line 45")),
	         qPrintable("scrollback is: " + console.toPlainText()));
}

void tst_QSimpleConsole::scrollbackCapPreservesPendingInput()
{
	QSimpleConsole console;
	console.show();
	console.setScrollbackLines(3);
	QTest::keyClicks(&console, "typing");

	for (int i = 0; i < 20; ++i)
		console.appendOutput(QStringLiteral("line %1").arg(i));

	QCOMPARE(console.getCommand(), QStringLiteral("typing"));
}

void tst_QSimpleConsole::clearRedrawsThePrompt()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "hello");
	QTest::keyClicks(&console, "half typed");

	console.clear();

	QCOMPARE(console.toPlainText(), QStringLiteral("> "));
	QCOMPARE(console.getCommand(), QString());
}

void tst_QSimpleConsole::clearLeavesTheConsoleUsable()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "hello");
	console.clear();

	QSignalSpy spy(&console, &QSimpleConsole::commandEntered);
	enter(&console, "after");

	QCOMPARE(spy.count(), 1);
	QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("after"));
	QCOMPARE(console.toPlainText(), QStringLiteral("> after\nafter\n> "));
}

void tst_QSimpleConsole::clearKeepsHistory()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "remembered");

	console.clear();
	QTest::keyClick(&console, Qt::Key_Up);

	QCOMPARE(console.getCommand(), QStringLiteral("remembered"));
}

void tst_QSimpleConsole::clearHistoryDropsRecordedCommands()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "secret");
	QTest::keyClicks(&console, "draft");

	console.clearHistory();
	QTest::keyClick(&console, Qt::Key_Up);

	// nothing to recall, so Up leaves the line alone
	QCOMPARE(console.getCommand(), QStringLiteral("draft"));
}

void tst_QSimpleConsole::upDownNavigatesHistory()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "one");
	enter(&console, "two");

	QTest::keyClick(&console, Qt::Key_Up);
	QCOMPARE(console.getCommand(), QStringLiteral("two"));
	QTest::keyClick(&console, Qt::Key_Up);
	QCOMPARE(console.getCommand(), QStringLiteral("one"));
	QTest::keyClick(&console, Qt::Key_Down);
	QCOMPARE(console.getCommand(), QStringLiteral("two"));
}

void tst_QSimpleConsole::historyIsCappedAtMaximum()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	for (int i = 0; i < 40; ++i)
		enter(&console, QStringLiteral("c%1").arg(i));

	for (int i = 0; i < 40; ++i)
		QTest::keyClick(&console, Qt::Key_Up);

	// 32 entries retained: c39 back to c8.
	QCOMPARE(console.getCommand(), QStringLiteral("c8"));
}

// ---------------------------------------------------------------------------
// regressions
// ---------------------------------------------------------------------------

void tst_QSimpleConsole::mouseSelectAllThenTypeKeepsPrompt()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "hello");

	QTextCursor cursor = console.textCursor();
	cursor.setPosition(0);
	cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
	console.setTextCursor(cursor);
	QTest::keyClicks(&console, "z");

	QVERIFY2(console.toPlainText().startsWith(QStringLiteral("> hello")),
	         qPrintable("document is now: " + console.toPlainText()));
}

void tst_QSimpleConsole::typingInsideOutputAppendsToInput()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "hello");
	const QString before = console.toPlainText();

	QTextCursor cursor = console.textCursor();
	cursor.setPosition(1); // inside the first prompt
	console.setTextCursor(cursor);
	QTest::keyClicks(&console, "XYZ");

	QCOMPARE(console.toPlainText(), before + QStringLiteral("XYZ"));
}

void tst_QSimpleConsole::commandStillEmittedAfterMouseSelectAll()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "hello");
	QSignalSpy spy(&console, &QSimpleConsole::commandEntered);

	QTextCursor cursor = console.textCursor();
	cursor.setPosition(0);
	cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
	console.setTextCursor(cursor);
	QTest::keyClicks(&console, "z");
	enter(&console, "hi");

	QCOMPARE(spy.count(), 1);
}

void tst_QSimpleConsole::setPromptRedrawsPrompt()
{
	QSimpleConsole console;
	console.show();

	console.setPrompt("$ ");

	QCOMPARE(console.toPlainText(), QStringLiteral("$ "));
}

void tst_QSimpleConsole::setPromptPreservesPendingCommand()
{
	QSimpleConsole console;
	console.show();
	QTest::keyClicks(&console, "abc");

	console.setPrompt("$ ");

	QCOMPARE(console.getCommand(), QStringLiteral("abc"));
}

void tst_QSimpleConsole::handlerMayPrefillNextCommand()
{
	QSimpleConsole console;
	console.show();
	connect(&console, &QSimpleConsole::commandEntered, &console, [&console](const QString &c) {
		console.appendOutput("OUT:" + c);
		console.setCurrentCommand("prefill");
	});

	enter(&console, "go");

	QCOMPARE(console.getCommand(), QStringLiteral("prefill"));
}

void tst_QSimpleConsole::multiLineCommandUsesNewlines()
{
	QSimpleConsole console;
	console.show();
	QSignalSpy spy(&console, &QSimpleConsole::commandEntered);

	// No input path produces a block break any more, but getCommand still has
	// to translate one if it ever appears.
	QTextCursor cursor = console.textCursor();
	cursor.movePosition(QTextCursor::End);
	cursor.insertText("aa");
	cursor.insertBlock();
	cursor.insertText("bb");
	console.setTextCursor(cursor);

	QTest::keyClick(&console, Qt::Key_Return);

	QCOMPARE(spy.count(), 1);
	const QString command = spy.at(0).at(0).toString();
	QVERIFY2(!command.contains(QChar::ParagraphSeparator),
	         "command still carries a U+2029 paragraph separator");
	QCOMPARE(command, QStringLiteral("aa\nbb"));
}

void tst_QSimpleConsole::shiftReturnDoesNotInsertRawNewline()
{
	QSimpleConsole console;
	console.show();
	QTest::keyClicks(&console, "abc");

	QTest::keyClick(&console, Qt::Key_Return, Qt::ShiftModifier);

	QVERIFY2(!console.toPlainText().endsWith('\n'),
	         qPrintable("document is now: " + console.toPlainText()));
}

void tst_QSimpleConsole::shiftBackspaceCannotDeletePrompt()
{
	QSimpleConsole console;
	console.show();

	for (int i = 0; i < 4; ++i)
		QTest::keyClick(&console, Qt::Key_Backspace, Qt::ShiftModifier);

	QCOMPARE(console.toPlainText(), QStringLiteral("> "));
}

void tst_QSimpleConsole::shiftHomeThenTypeKeepsPrompt()
{
	QSimpleConsole console;
	console.show();
	QTest::keyClicks(&console, "hello");

	QTest::keyClick(&console, Qt::Key_Home, Qt::ShiftModifier);
	QTest::keyClicks(&console, "z");

	QCOMPARE(console.toPlainText(), QStringLiteral("> z"));
}

void tst_QSimpleConsole::shiftInsertRespectsPromptBoundary()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "hello");
	QApplication::clipboard()->setText("PASTED");
	QTextCursor cursor = console.textCursor();
	cursor.setPosition(1); // inside the scrollback
	console.setTextCursor(cursor);

	QTest::keyClick(&console, Qt::Key_Insert, Qt::ShiftModifier);

	QCOMPARE(console.toPlainText(), QStringLiteral("> hello\nhello\n> PASTED"));
	QCOMPARE(console.getCommand(), QStringLiteral("PASTED"));
}

void tst_QSimpleConsole::asyncOutputPreservesWrappedInput()
{
	QSimpleConsole console;
	console.resize(120, 200); // narrow enough to wrap the input line
	console.show();
	QTest::keyClicks(&console, "aaaa bbbb cccc dddd eeee ffff gggg hhhh");

	console.appendOutput("ASYNC");

	QCOMPARE(console.toPlainText(),
	         QStringLiteral("ASYNC\n> aaaa bbbb cccc dddd eeee ffff gggg hhhh"));
}

void tst_QSimpleConsole::silentHandlerPreservesPendingInput()
{
	QSimpleConsole console;
	console.show();
	connect(&console, &QSimpleConsole::commandEntered, &console, [](const QString &) {
		/* a handler that consumes the command without writing output */
	});
	enter(&console, "silent");
	QTest::keyClicks(&console, "typing");

	console.appendOutput("LATE");

	QCOMPARE(console.getCommand(), QStringLiteral("typing"));
}

void tst_QSimpleConsole::ctrlBackspaceCannotDeleteAlphanumericPrompt()
{
	QSimpleConsole console;
	console.show();
	console.setPrompt("abc");
	console.appendOutput("banner"); // draws the new prompt (see F02)
	QTest::keyClicks(&console, "foo");

	QTest::keyClick(&console, Qt::Key_Backspace, Qt::ControlModifier);

	QVERIFY2(console.toPlainText().endsWith(QStringLiteral("abc")),
	         qPrintable("document is now: " + console.toPlainText()));
}

void tst_QSimpleConsole::cutWithoutSelectionKeepsClipboard()
{
	QSimpleConsole console;
	console.show();
	QApplication::clipboard()->setText("PRECIOUS");
	QTest::keyClicks(&console, "zz");

	QTest::keyClick(&console, Qt::Key_X, Qt::ControlModifier);

	QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("PRECIOUS"));
}

void tst_QSimpleConsole::pastedNewlinesStayOnOneLine()
{
	QSimpleConsole console;
	console.show();
	QApplication::clipboard()->setText("aa\nbb\n");

	QTest::keyClick(&console, Qt::Key_V, Qt::ControlModifier);

	QCOMPARE(console.document()->blockCount(), 1);
	QCOMPARE(console.toPlainText(), QStringLiteral("> aa bb"));
	QCOMPARE(console.getCommand(), QStringLiteral("aa bb"));
}

void tst_QSimpleConsole::droppedTextLandsInInput()
{
	PastableConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "hello");
	const QString before = console.toPlainText();

	QTextCursor cursor = console.textCursor();
	cursor.setPosition(1); // where a drop into the scrollback lands
	console.setTextCursor(cursor);

	QMimeData mime;
	mime.setText("DROPPED");
	console.insertFromMimeData(&mime);

	QCOMPARE(console.toPlainText(), before + QStringLiteral("DROPPED"));
	QCOMPARE(console.getCommand(), QStringLiteral("DROPPED"));
}

void tst_QSimpleConsole::historyRestoresDraftLine()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "one");
	QTest::keyClicks(&console, "draft");

	QTest::keyClick(&console, Qt::Key_Up);
	QTest::keyClick(&console, Qt::Key_Down);

	QCOMPARE(console.getCommand(), QStringLiteral("draft"));
}

void tst_QSimpleConsole::historySkipsConsecutiveDuplicates()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "one");
	enter(&console, "two");
	enter(&console, "two");

	QTest::keyClick(&console, Qt::Key_Up);
	QCOMPARE(console.getCommand(), QStringLiteral("two"));
	QTest::keyClick(&console, Qt::Key_Up);

	QCOMPARE(console.getCommand(), QStringLiteral("one"));
}

QTEST_MAIN(tst_QSimpleConsole)

#include "tst_qsimpleconsole.moc"
