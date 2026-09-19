#include <QtTest>

#include <QApplication>
#include <QClipboard>
#include <QSignalSpy>
#include <QTextBlock>

#include "qsimpleconsole.h"

/*
 * Two kinds of test live here.
 *
 *  - Regressions: behaviour that is wrong today. Each is marked QEXPECT_FAIL
 *    with the finding id from FINDINGS.html, so the suite is green as it
 *    stands. When a fix lands the test reports an unexpected pass, which
 *    fails the run until the marker is removed - the fix cannot be claimed
 *    without the evidence.
 *
 *  - Characterization: behaviour that is correct today and must survive the
 *    prompt-boundary rework (F15). These carry no marker and must always pass.
 */
class tst_QSimpleConsole : public QObject
{
	Q_OBJECT

private slots:
	// -- characterization: the prompt boundary ---------------------------
	void promptIsDrawnOnConstruction();
	void typedTextIsEmittedAsCommand();
	void emptyReturnDrawsNewPrompt();
	void homeMovesToPromptEnd();
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

	// -- characterization: history ---------------------------------------
	void upDownNavigatesHistory();
	void historyIsCappedAtMaximum();

	// -- regressions ------------------------------------------------------
	void mouseSelectAllThenTypeKeepsDocument();          // F01
	void typingInsideOutputDoesNotCorruptDocument();     // F01
	void commandStillEmittedAfterMouseSelectAll();       // F01
	void setPromptRedrawsPrompt();                       // F02
	void setPromptPreservesPendingCommand();             // F02
	void handlerMayPrefillNextCommand();                 // F03
	void multiLineCommandUsesNewlines();                 // F04
	void shiftReturnDoesNotInsertRawNewline();           // F05
	void shiftBackspaceCannotDeletePrompt();             // F05
	void shiftHomeThenTypeKeepsPrompt();                 // F05
	void shiftInsertRespectsPromptBoundary();            // F06
	void asyncOutputPreservesWrappedInput();             // F07
	void silentHandlerKeepsInputRetentionWorking();      // F08
	void ctrlBackspaceCannotDeleteAlphanumericPrompt();  // F09
	void cutWithoutSelectionKeepsClipboard();            // F10
	void pastedNewlinesDoNotBreakThePromptLine();        // F11
	void historyRestoresDraftLine();                     // F21
	void historySkipsConsecutiveDuplicates();            // F21
};

namespace {

/* Echoes every command back, which is what the README's example does and
 * what most consumers will do. */
void echoCommands(QSimpleConsole *console)
{
	QObject::connect(console, &QSimpleConsole::commandEntered,
	                 console, &QSimpleConsole::appendOutput);
}

void enter(QSimpleConsole *console, const QString &command)
{
	QTest::keyClicks(console, command);
	QTest::keyClick(console, Qt::Key_Return);
}

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

void tst_QSimpleConsole::homeMovesToPromptEnd()
{
	QSimpleConsole console;
	console.show();
	QTest::keyClicks(&console, "abc");

	QTest::keyClick(&console, Qt::Key_Home);

	QCOMPARE(console.textCursor().position(), 2);   // just past "> "
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

	const int promptEnd = console.document()->lastBlock().position() + 2;
	QCOMPARE(console.textCursor().position(), promptEnd);
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

void tst_QSimpleConsole::mouseSelectAllThenTypeKeepsDocument()
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

	QEXPECT_FAIL("", "F01: a printable key with a selection crossing the prompt "
	                 "wipes the prompt and the scrollback", Abort);
	QVERIFY2(console.toPlainText().startsWith(QStringLiteral("> hello")),
	         qPrintable("document is now: " + console.toPlainText()));
}

void tst_QSimpleConsole::typingInsideOutputDoesNotCorruptDocument()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "hello");
	const QString before = console.toPlainText();

	QTextCursor cursor = console.textCursor();
	cursor.setPosition(1);              // inside the first prompt
	console.setTextCursor(cursor);
	QTest::keyClicks(&console, "XYZ");

	QEXPECT_FAIL("", "F01: typing before the prompt edits the scrollback", Abort);
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

	QEXPECT_FAIL("", "F01: the boundary is left out of range, so Return silently "
	                 "drops every later command", Abort);
	QCOMPARE(spy.count(), 1);
}

void tst_QSimpleConsole::setPromptRedrawsPrompt()
{
	QSimpleConsole console;
	console.show();

	console.setPrompt("$ ");

	QEXPECT_FAIL("", "F02: setPrompt never touches the document, so the new "
	                 "prompt appears only after the next output", Abort);
	QCOMPARE(console.toPlainText(), QStringLiteral("$ "));
}

void tst_QSimpleConsole::setPromptPreservesPendingCommand()
{
	QSimpleConsole console;
	console.show();
	QTest::keyClicks(&console, "abc");

	console.setPrompt("$ ");

	QEXPECT_FAIL("", "F02: the boundary is set past the typed text, which is "
	                 "then swallowed into the prompt", Abort);
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

	QEXPECT_FAIL("", "F03: handleReturnKey overwrites the boundary after the "
	                 "emit, stranding anything the handler set up", Abort);
	QCOMPARE(console.getCommand(), QStringLiteral("prefill"));
}

void tst_QSimpleConsole::multiLineCommandUsesNewlines()
{
	QSimpleConsole console;
	console.show();
	QSignalSpy spy(&console, &QSimpleConsole::commandEntered);
	QApplication::clipboard()->setText("aa\nbb");

	QTest::keyClick(&console, Qt::Key_V, Qt::ControlModifier);
	QTest::keyClick(&console, Qt::Key_Return);

	QCOMPARE(spy.count(), 1);
	const QString command = spy.at(0).at(0).toString();
	QEXPECT_FAIL("", "F04: getCommand replaces U+2028 but selectedText emits "
	                 "U+2029 for block breaks", Abort);
	QVERIFY2(!command.contains(QChar::ParagraphSeparator),
	         "command still carries a U+2029 paragraph separator");
}

void tst_QSimpleConsole::shiftReturnDoesNotInsertRawNewline()
{
	QSimpleConsole console;
	console.show();
	QTest::keyClicks(&console, "abc");

	QTest::keyClick(&console, Qt::Key_Return, Qt::ShiftModifier);

	QEXPECT_FAIL("", "F05: the Shift branch runs before the switch, so Return "
	                 "is never handled and a bare newline is inserted", Abort);
	QVERIFY2(!console.toPlainText().endsWith('\n'),
	         qPrintable("document is now: " + console.toPlainText()));
}

void tst_QSimpleConsole::shiftBackspaceCannotDeletePrompt()
{
	QSimpleConsole console;
	console.show();

	for (int i = 0; i < 4; ++i)
		QTest::keyClick(&console, Qt::Key_Backspace, Qt::ShiftModifier);

	QEXPECT_FAIL("", "F05: the Shift branch bypasses the Backspace guard", Abort);
	QCOMPARE(console.toPlainText(), QStringLiteral("> "));
}

void tst_QSimpleConsole::shiftHomeThenTypeKeepsPrompt()
{
	QSimpleConsole console;
	console.show();
	QTest::keyClicks(&console, "hello");

	QTest::keyClick(&console, Qt::Key_Home, Qt::ShiftModifier);
	QTest::keyClicks(&console, "z");

	QEXPECT_FAIL("", "F05: Shift+Home selects back across the prompt and the "
	                 "next keystroke replaces it", Abort);
	QCOMPARE(console.toPlainText(), QStringLiteral("> z"));
}

void tst_QSimpleConsole::shiftInsertRespectsPromptBoundary()
{
	QSimpleConsole console;
	console.show();
	QApplication::clipboard()->setText("PASTED");
	QTextCursor cursor = console.textCursor();
	cursor.setPosition(0);
	console.setTextCursor(cursor);

	QTest::keyClick(&console, Qt::Key_Insert, Qt::ShiftModifier);

	QEXPECT_FAIL("", "F06: modifiers == ControlModifier misses Shift+Insert, "
	                 "the platform's other paste binding", Abort);
	QCOMPARE(console.toPlainText(), QStringLiteral("> "));
}

void tst_QSimpleConsole::asyncOutputPreservesWrappedInput()
{
	QSimpleConsole console;
	console.resize(120, 200);           // narrow enough to wrap the input line
	console.show();
	QTest::keyClicks(&console, "aaaa bbbb cccc dddd eeee ffff gggg hhhh");

	console.appendOutput("ASYNC");

	QEXPECT_FAIL("", "F07: LineUnderCursor removes a visual line, not a block, "
	                 "so the wrapped remainder is left glued to the output", Abort);
	QCOMPARE(console.toPlainText(),
	         QStringLiteral("ASYNC\n> aaaa bbbb cccc dddd eeee ffff gggg hhhh"));
}

void tst_QSimpleConsole::silentHandlerKeepsInputRetentionWorking()
{
	QSimpleConsole console;
	console.show();
	connect(&console, &QSimpleConsole::commandEntered, &console, [](const QString &) {
		/* a handler that consumes the command without writing output */
	});
	enter(&console, "silent");
	QTest::keyClicks(&console, "typing");

	console.appendOutput("LATE");

	QEXPECT_FAIL("", "F08: isUserInput is never cleared, so the next async "
	                 "output discards the pending input", Abort);
	QCOMPARE(console.getCommand(), QStringLiteral("typing"));
}

void tst_QSimpleConsole::ctrlBackspaceCannotDeleteAlphanumericPrompt()
{
	QSimpleConsole console;
	console.show();
	console.setPrompt("abc");
	console.appendOutput("banner");     // draws the new prompt (see F02)
	QTest::keyClicks(&console, "foo");

	QTest::keyClick(&console, Qt::Key_Backspace, Qt::ControlModifier);

	QEXPECT_FAIL("", "F09: the word-wise delete crosses the boundary when the "
	                 "prompt has no trailing punctuation", Abort);
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

	QEXPECT_FAIL("", "F10: handleCut writes the empty selection to the "
	                 "clipboard before checking hasSelection", Abort);
	QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("PRECIOUS"));
}

void tst_QSimpleConsole::pastedNewlinesDoNotBreakThePromptLine()
{
	QSimpleConsole console;
	console.show();
	QApplication::clipboard()->setText("aa\nbb");

	QTest::keyClick(&console, Qt::Key_V, Qt::ControlModifier);

	QEXPECT_FAIL("", "F11: pasted newlines create blocks with no prompt", Abort);
	QCOMPARE(console.document()->blockCount(), 1);
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

	QEXPECT_FAIL("", "F21: the draft line is discarded by the first Up and "
	                 "never restored", Abort);
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

	QEXPECT_FAIL("", "F21: consecutive duplicates are stored twice, so Up "
	                 "appears not to move", Abort);
	QCOMPARE(console.getCommand(), QStringLiteral("one"));
}

QTEST_MAIN(tst_QSimpleConsole)

#include "tst_qsimpleconsole.moc"
