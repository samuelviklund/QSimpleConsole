#include <QtTest>

#include <cstdint>

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QSignalSpy>
#include <QTemporaryDir>
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

	// -- characterization: the output path -------------------------------
	void appendOutputLineEndings_data();
	void appendOutputLineEndings();

	// -- characterization: Home and End ----------------------------------
	void ctrlHomeMovesToDocumentStart();
	void ctrlShiftHomeSelectsToDocumentStart();
	void plainEndMovesToInputEnd();
	void endCrossesAWrappedLine();
	void shiftEndSelectsToInputEnd();
	void ctrlEndMovesToDocumentEnd();

	// -- characterization: output ----------------------------------------
	void asyncOutputPreservesPendingInput();
	void twoOutputsForOneCommandRenderCleanly();
	void timestampPrefixesOutput();
	void timestampPrefixesEveryLine();
	void timestampSkipsBlankLines();
	void maximumBlockCountDoesNotBreakBoundary();
	void scrollbackIsUnlimitedByDefault();
	void scrollbackLinesCapsHistory();
	void scrollbackCapPreservesPendingInput();
	void clearRedrawsThePrompt();
	void clearLeavesTheConsoleUsable();
	void clearKeepsHistory();
	void clearHistoryDropsRecordedCommands();
	void currentCommandLeavesTheCursorAlone();
	void propertiesReachTheMetaObject();

	// -- characterization: saving ----------------------------------------
	void saveToFileWritesTheDocument();
	void saveToFileReportsFailure();

	// -- characterization: history ---------------------------------------
	void upDownNavigatesHistory();
	void historyIsCappedAtMaximum();

	// -- characterization: the history API -------------------------------
	void historyIsNewestFirst();
	void setHistoryRoundTrips();
	void setHistoryTruncatesToTheMaximum();
	void setHistoryEndsAWalkInProgress();
	void restoredHistoryWalksWithUpAndDown();
	void loweringTheMaximumDropsTheOldest();
	void maximumHistorySizeOfZeroDisablesHistory();

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
	QCOMPARE(console.currentCommand(), QString());
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

	for (int32_t i = 0; i < 4; ++i)
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

	for (int32_t i = 0; i < 6; ++i)
		QTest::keyClick(&console, Qt::Key_Left, Qt::ControlModifier);

	const int32_t inputStart = console.document()->lastBlock().position() + 2;
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
// characterization: the output path
// ---------------------------------------------------------------------------

/* appendOutput takes whatever a producer hands it. A terminator is stripped so
 * the prompt below does not leave a blank line, but only one, so a caller that
 * asked for a blank line still gets it. */
void tst_QSimpleConsole::appendOutputLineEndings_data()
{
	QTest::addColumn<QString>("output");
	QTest::addColumn<QString>("expected");

	QTest::newRow("plain") << "line" << "line\n> ";
	QTest::newRow("lf") << "line\n" << "line\n> ";
	QTest::newRow("crlf") << "line\r\n" << "line\n> ";
	QTest::newRow("cr") << "line\r" << "line\n> ";
	QTest::newRow("blank line kept") << "line\n\n" << "line\n\n> ";
	QTest::newRow("embedded") << "a\nb" << "a\nb\n> ";
	QTest::newRow("empty") << "" << "\n> ";
	QTest::newRow("newline only") << "\n" << "\n> ";
	QTest::newRow("whitespace") << "  " << "  \n> ";
}

void tst_QSimpleConsole::appendOutputLineEndings()
{
	QFETCH(QString, output);
	QFETCH(QString, expected);

	QSimpleConsole console;
	console.show();
	console.clear();

	console.appendOutput(output);

	QCOMPARE(console.toPlainText(), expected);
}

// ---------------------------------------------------------------------------
// characterization: Home and End
// ---------------------------------------------------------------------------

void tst_QSimpleConsole::ctrlHomeMovesToDocumentStart()
{
	QSimpleConsole console;
	console.show();
	console.appendOutput("line one");
	console.appendOutput("line two");

	QTest::keyClick(&console, Qt::Key_Home, Qt::ControlModifier);

	// Plain Home stops at the input; Ctrl+Home is the only keyboard route to
	// the top of the scrollback.
	QCOMPARE(console.textCursor().position(), 0);
}

void tst_QSimpleConsole::ctrlShiftHomeSelectsToDocumentStart()
{
	QSimpleConsole console;
	console.show();
	console.appendOutput("line one");
	QTest::keyClicks(&console, "abc");

	QTest::keyClick(&console, Qt::Key_Home, Qt::ControlModifier | Qt::ShiftModifier);

	// Compared as an extent, not as text: selectedText() encodes the block
	// break as U+2029, which is what currentCommand() exists to translate.
	QCOMPARE(console.textCursor().selectionStart(), 0);
	QCOMPARE(console.textCursor().selectionEnd(), console.document()->characterCount() - 1);
}

void tst_QSimpleConsole::plainEndMovesToInputEnd()
{
	QSimpleConsole console;
	console.show();
	QTest::keyClicks(&console, "abc");
	const int32_t end = console.textCursor().position();

	QTest::keyClick(&console, Qt::Key_Home);
	QTest::keyClick(&console, Qt::Key_End);

	QCOMPARE(console.textCursor().position(), end);
}

void tst_QSimpleConsole::endCrossesAWrappedLine()
{
	QSimpleConsole console;
	console.setLineWrapMode(QPlainTextEdit::WidgetWidth);
	console.resize(120, 200);
	console.show();
	QTest::keyClicks(&console, QString("word ").repeated(12).trimmed());
	const int32_t end = console.textCursor().position();

	QTest::keyClick(&console, Qt::Key_Home);
	QTest::keyClick(&console, Qt::Key_End);

	// Qt's own End stops at the wrap. Home spans the whole input, so End has to
	// as well, or the pair disagrees about where the input line ends.
	QCOMPARE(console.textCursor().position(), end);
}

void tst_QSimpleConsole::shiftEndSelectsToInputEnd()
{
	QSimpleConsole console;
	console.show();
	QTest::keyClicks(&console, "abc");
	QTest::keyClick(&console, Qt::Key_Home);

	QTest::keyClick(&console, Qt::Key_End, Qt::ShiftModifier);

	QCOMPARE(console.textCursor().selectedText(), QStringLiteral("abc"));
}

void tst_QSimpleConsole::ctrlEndMovesToDocumentEnd()
{
	QSimpleConsole console;
	console.show();
	console.appendOutput("line one");
	QTest::keyClicks(&console, "abc");
	QTest::keyClick(&console, Qt::Key_Home, Qt::ControlModifier);

	QTest::keyClick(&console, Qt::Key_End, Qt::ControlModifier);

	QCOMPARE(console.textCursor().position(), console.document()->characterCount() - 1);
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
	QCOMPARE(console.currentCommand(), QStringLiteral("typing"));
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
	console.setTimestampEnabled(true);

	console.appendOutput("hello");

	const QStringList lines = console.toPlainText().split('\n');
	QVERIFY(lines.size() >= 2);
	QVERIFY2(QRegularExpression("^\\d{2}:\\d{2}:\\d{2}: hello$").match(lines.at(0)).hasMatch(),
	         qPrintable("unexpected output line: " + lines.at(0)));
}

/* Output handed over in one call can still be several lines. Every one of them
 * carries the stamp, or the ones below the first look like continuations of a
 * line that was never timestamped. */
void tst_QSimpleConsole::timestampPrefixesEveryLine()
{
	QSimpleConsole console;
	console.show();
	console.setTimestampEnabled(true);

	console.appendOutput("alpha\nbeta\ngamma");

	const QStringList lines = console.toPlainText().split('\n');
	const QRegularExpression stamped("^\\d{2}:\\d{2}:\\d{2}: (alpha|beta|gamma)$");
	QCOMPARE(lines.size(), 4); // three output lines plus the prompt
	for (int32_t i = 0; i < 3; ++i)
		QVERIFY2(stamped.match(lines.at(i)).hasMatch(),
		         qPrintable("unstamped line: " + lines.at(i)));
}

void tst_QSimpleConsole::timestampSkipsBlankLines()
{
	QSimpleConsole console;
	console.show();
	console.setTimestampEnabled(true);

	console.appendOutput("alpha\n\nbeta");

	const QStringList lines = console.toPlainText().split('\n');
	QCOMPARE(lines.size(), 4);
	QVERIFY(lines.at(0).endsWith(": alpha"));
	QCOMPARE(lines.at(1), QString()); // no bare stamp on its own line
	QVERIFY(lines.at(2).endsWith(": beta"));
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

	QCOMPARE(console.currentCommand(), QStringLiteral("next"));
}

// ---------------------------------------------------------------------------
// characterization: saving
// ---------------------------------------------------------------------------

void tst_QSimpleConsole::saveToFileWritesTheDocument()
{
	QSimpleConsole console;
	echoCommands(&console);
	enter(&console, "first");
	enter(&console, "second");
	QTest::keyClicks(&console, "half typed");

	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString path = dir.filePath("console.txt");

	QSignalSpy errors(&console, &QSimpleConsole::saveError);
	QVERIFY(console.saveToFile(path));
	QCOMPARE(errors.count(), 0);

	QFile file(path);
	QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));

	// The file carries the console verbatim, pending input and all, with one
	// closing newline that the document itself does not have.
	QCOMPARE(QString::fromUtf8(file.readAll()), console.toPlainText() + "\n");
}

void tst_QSimpleConsole::saveToFileReportsFailure()
{
	QSimpleConsole console;

	QTemporaryDir dir;
	QVERIFY(dir.isValid());

	QSignalSpy errors(&console, &QSimpleConsole::saveError);
	QVERIFY(!console.saveToFile(dir.filePath("no/such/directory/console.txt")));
	QCOMPARE(errors.count(), 1);
	QVERIFY(!errors.at(0).at(0).toString().isEmpty());
}

// ---------------------------------------------------------------------------
// characterization: history
// ---------------------------------------------------------------------------

void tst_QSimpleConsole::scrollbackIsUnlimitedByDefault()
{
	QSimpleConsole console;
	console.show();

	QCOMPARE(console.scrollbackLines(), 0);

	for (int32_t i = 0; i < 200; ++i)
		console.appendOutput(QStringLiteral("line %1").arg(i));

	QCOMPARE(console.document()->blockCount(), 201);
}

void tst_QSimpleConsole::scrollbackLinesCapsHistory()
{
	QSimpleConsole console;
	console.show();
	console.setScrollbackLines(5);

	QCOMPARE(console.scrollbackLines(), 5);

	for (int32_t i = 0; i < 50; ++i)
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

	for (int32_t i = 0; i < 20; ++i)
		console.appendOutput(QStringLiteral("line %1").arg(i));

	QCOMPARE(console.currentCommand(), QStringLiteral("typing"));
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
	QCOMPARE(console.currentCommand(), QString());
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

	QCOMPARE(console.currentCommand(), QStringLiteral("remembered"));
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
	QCOMPARE(console.currentCommand(), QStringLiteral("draft"));
}

void tst_QSimpleConsole::currentCommandLeavesTheCursorAlone()
{
	QSimpleConsole console;
	console.show();
	QTest::keyClicks(&console, "abc");

	// park the cursor mid-command, where reading must not disturb it
	QTextCursor cursor = console.textCursor();
	cursor.setPosition(console.document()->lastBlock().position() + 3);
	console.setTextCursor(cursor);
	const int32_t position = console.textCursor().position();

	QCOMPARE(console.currentCommand(), QStringLiteral("abc"));

	QCOMPARE(console.textCursor().position(), position);
	QVERIFY(!console.textCursor().hasSelection());
}

void tst_QSimpleConsole::propertiesReachTheMetaObject()
{
	QSimpleConsole console;
	console.show();

	QVERIFY(console.setProperty("prompt", QStringLiteral("$ ")));
	QCOMPARE(console.property("prompt").toString(), QStringLiteral("$ "));
	QCOMPARE(console.toPlainText(), QStringLiteral("$ "));

	QVERIFY(console.setProperty("timestampEnabled", true));
	QVERIFY(console.property("timestampEnabled").toBool());

	QVERIFY(console.setProperty("scrollbackLines", 100));
	QCOMPARE(console.property("scrollbackLines").toInt(), 100);

	QVERIFY(console.setProperty("maximumHistorySize", 8u));
	QCOMPARE(console.property("maximumHistorySize").toUInt(), 8u);

	QVERIFY(console.setProperty("history", QStringList({ "a", "b" })));
	QCOMPARE(console.property("history").toStringList(), QStringList({ "a", "b" }));
}

void tst_QSimpleConsole::upDownNavigatesHistory()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "one");
	enter(&console, "two");

	QTest::keyClick(&console, Qt::Key_Up);
	QCOMPARE(console.currentCommand(), QStringLiteral("two"));
	QTest::keyClick(&console, Qt::Key_Up);
	QCOMPARE(console.currentCommand(), QStringLiteral("one"));
	QTest::keyClick(&console, Qt::Key_Down);
	QCOMPARE(console.currentCommand(), QStringLiteral("two"));
}

void tst_QSimpleConsole::historyIsCappedAtMaximum()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	for (int32_t i = 0; i < 40; ++i)
		enter(&console, QStringLiteral("c%1").arg(i));

	for (int32_t i = 0; i < 40; ++i)
		QTest::keyClick(&console, Qt::Key_Up);

	// 32 entries retained: c39 back to c8.
	QCOMPARE(console.currentCommand(), QStringLiteral("c8"));
}

// ---------------------------------------------------------------------------
// characterization: the history API
// ---------------------------------------------------------------------------

void tst_QSimpleConsole::historyIsNewestFirst()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "first");
	enter(&console, "second");

	// Index 0 is what the first Up recalls.
	QCOMPARE(console.history(), QStringList({ "second", "first" }));
}

void tst_QSimpleConsole::setHistoryRoundTrips()
{
	QSimpleConsole console;
	console.show();
	const QStringList saved = { "newest", "middle", "oldest" };

	console.setHistory(saved);

	QCOMPARE(console.history(), saved);
}

void tst_QSimpleConsole::setHistoryTruncatesToTheMaximum()
{
	QSimpleConsole console;
	console.show();
	console.setMaximumHistorySize(2);

	console.setHistory({ "a", "b", "c", "d" });

	// The newest survive; the cap stays the authority on how much is kept.
	QCOMPARE(console.history(), QStringList({ "a", "b" }));
}

void tst_QSimpleConsole::setHistoryEndsAWalkInProgress()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	enter(&console, "old");
	QTest::keyClick(&console, Qt::Key_Up);
	QCOMPARE(console.currentCommand(), QStringLiteral("old"));

	console.setHistory({ "restored" });

	// The walk is over, so the next Up starts from the newest entry of the new
	// list rather than stepping past where it had reached in the old one.
	QTest::keyClick(&console, Qt::Key_Up);
	QCOMPARE(console.currentCommand(), QStringLiteral("restored"));
}

void tst_QSimpleConsole::restoredHistoryWalksWithUpAndDown()
{
	QSimpleConsole console;
	console.show();
	console.setHistory({ "two", "one" });

	QTest::keyClick(&console, Qt::Key_Up);
	QCOMPARE(console.currentCommand(), QStringLiteral("two"));
	QTest::keyClick(&console, Qt::Key_Up);
	QCOMPARE(console.currentCommand(), QStringLiteral("one"));
	QTest::keyClick(&console, Qt::Key_Down);
	QCOMPARE(console.currentCommand(), QStringLiteral("two"));
}

void tst_QSimpleConsole::loweringTheMaximumDropsTheOldest()
{
	QSimpleConsole console;
	console.show();
	console.setHistory({ "d", "c", "b", "a" });

	console.setMaximumHistorySize(2);

	// Takes effect at once, not on the next command entered.
	QCOMPARE(console.history(), QStringList({ "d", "c" }));
}

void tst_QSimpleConsole::maximumHistorySizeOfZeroDisablesHistory()
{
	QSimpleConsole console;
	console.show();
	echoCommands(&console);
	console.setMaximumHistorySize(0);

	enter(&console, "vanishes");

	QVERIFY(console.history().isEmpty());
	QTest::keyClick(&console, Qt::Key_Up);
	QVERIFY(console.currentCommand().isEmpty());
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

	QCOMPARE(console.currentCommand(), QStringLiteral("abc"));
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

	QCOMPARE(console.currentCommand(), QStringLiteral("prefill"));
}

void tst_QSimpleConsole::multiLineCommandUsesNewlines()
{
	QSimpleConsole console;
	console.show();
	QSignalSpy spy(&console, &QSimpleConsole::commandEntered);

	// No input path produces a block break any more, but currentCommand still has
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

	for (int32_t i = 0; i < 4; ++i)
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
	QCOMPARE(console.currentCommand(), QStringLiteral("PASTED"));
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

	QCOMPARE(console.currentCommand(), QStringLiteral("typing"));
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
	QCOMPARE(console.currentCommand(), QStringLiteral("aa bb"));
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
	QCOMPARE(console.currentCommand(), QStringLiteral("DROPPED"));
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

	QCOMPARE(console.currentCommand(), QStringLiteral("draft"));
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
	QCOMPARE(console.currentCommand(), QStringLiteral("two"));
	QTest::keyClick(&console, Qt::Key_Up);

	QCOMPARE(console.currentCommand(), QStringLiteral("one"));
}

QTEST_MAIN(tst_QSimpleConsole)

#include "tst_qsimpleconsole.moc"
