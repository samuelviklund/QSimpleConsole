# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.4.0] - 2026-10-10

### Added

- `history()` and `setHistory()` read and replace the recorded commands, so an
  application can save the history and restore it on the next run. The list is
  newest first, which is the order `Up` walks.
- `maximumHistorySize()` and `setMaximumHistorySize()` cap how many commands
  are kept. The cap was fixed at 32 and private. Lowering it drops the oldest
  at once, and a value of 0 disables the history.
- `interruptRequested()` is emitted when `Ctrl+C` is pressed with nothing
  selected. The widget keeps no notion of a running command, so it reports the
  request and nothing more; with a selection, `Ctrl+C` is still Copy.
- `completionRequested()` is emitted on `Tab`, carrying the input typed so far.
  Answer it with `setCurrentCommand()`.
- `history` and `maximumHistorySize` are declared as `Q_PROPERTY`, joining the
  three existing properties.

### Changed

- Every line of a multi-line `appendOutput()` carries the timestamp. Only the
  first line used to, so the rest read as continuations of a line that had
  never been stamped. Blank lines stay bare. **This changes the output of any
  caller that passes embedded newlines with timestamps enabled.**
- `Tab` no longer inserts a literal tab character into the command. With
  nothing connected to `completionRequested()` it now does nothing. `Ctrl+Tab`
  is left to the host application.

### Removed

- The history is stored in a `QStringList` rather than a `std::deque`, which
  removes the `<deque>` and `<cstddef>` includes from the public header.

## [1.3.1] - 2026-10-06

### Fixed

- `appendOutput()` strips one trailing line terminator, so output that arrives
  newline-terminated (as everything read from a `QProcess` does) no longer
  leaves a blank line between every pair of lines. Exactly one terminator is
  removed, and CRLF counts as one, so a caller that asks for a blank line still
  gets it.
- `Ctrl+Home` reaches the top of the scrollback. Every `Home` variant used to
  land at the start of the input, which left no way to get there from the
  keyboard. `Ctrl+Shift+Home` selects to the start of the document.
- `End` moves to the end of the input rather than the end of the visual line,
  matching `Home`. On a wrapped input line the two used to disagree about where
  the line ended. `Ctrl+End` still goes to the end of the document.

## [1.3.0] - 2026-10-05

### Added

- A **Clear** entry in the context menu, which calls `clear()`. The slot has
  been there since 1.1.0 with no way for the user to reach it.
- `saveToFile()` writes the console verbatim to a file, with a closing newline.
  A failed write returns `false` and emits the new `saveError()` signal; the
  widget shows nothing itself, so the reporting is the consumer's to decide.
- A **Save As...** entry in the context menu, which asks for a path and calls
  `saveToFile()`.

## [1.2.0] - 2026-09-19

### Added

- `currentCommand()` replaces `getCommand()`. It is `const` and free of side
  effects; the old name moved the visible cursor and selection as a side
  effect of being read.
- `setTimestampEnabled()` and `isTimestampEnabled()` replace `setTimestamp()`,
  which had no reader.
- `prompt()` reads the prompt back.
- `prompt`, `timestampEnabled` and `scrollbackLines` are declared as
  `Q_PROPERTY`, so they are reachable from Designer, QML and `setProperty()`.
- `QSIMPLECONSOLE_BUILD_EXAMPLES` builds `examples/minimal` from the top-level
  project, alongside the existing `QSIMPLECONSOLE_BUILD_TESTS`.

### Changed

- The console asks the platform for its fixed-width font instead of naming
  "Source Code Pro". A missing family fell back to a proportional face; on a
  stock system the console was not monospaced at all. Call `setFont()` if you
  want a particular family.
- The exported CMake target carries its C++17 requirement, so
  `find_package(QSimpleConsole)` consumers no longer have to set the standard
  themselves.

### Deprecated

- `getCommand()` and `setTimestamp()`. Both still compile and forward to the
  new names. They will be removed in 2.0.0.

## [1.1.0] - 2026-09-19

The prompt boundary was previously cached as an absolute character offset into
the document, maintained by hand from four places and invalidated by any edit
those places did not know about. It is now derived from the document, which is
the root cause behind most of the fixes below.

### Added

- `QSimpleConsole` takes an optional parent widget, like every other `QWidget`
  subclass: `new QSimpleConsole(this)`. Constructing without one still works.
- `setScrollbackLines()` / `scrollbackLines()` cap the output kept above the
  input line. Unlimited remains the default; note that capping the scrollback
  disables undo/redo.
- `clear()` empties the console and draws a fresh prompt, and `clearHistory()`
  drops the recorded commands. Note that `clear()` hides `QPlainTextEdit`'s
  non-virtual slot of the same name rather than overriding it.
- Install and export rules, so the library can be consumed with
  `find_package(QSimpleConsole)` instead of being vendored. Both that and
  `add_subdirectory()` provide the same `QSimpleConsole::qsimpleconsole`
  target.
- Qt Test suite covering the prompt boundary, output rendering and history.
  Build it with `-DQSIMPLECONSOLE_BUILD_TESTS=ON` and run it through CTest; it
  needs no display server.

### Fixed

- Typing, pasting or dropping text while the cursor or selection sits outside
  the input line no longer edits or erases the scrollback. Previously a
  select-all with the mouse followed by one keystroke emptied the console and
  left it unable to submit any further command.
- `setPrompt()` now redraws the prompt instead of leaving the old one in place
  until the next output, and no longer swallows text that had already been
  typed.
- A slot connected to `commandEntered` can now change console state (writing
  output, pre-filling the next command) without having its work overwritten
  when the signal returns.
- Multi-line commands are delivered with `\n` separators. They previously
  carried the U+2029 paragraph separator that `QTextCursor::selectedText()`
  produces, which no split on a newline would find.
- Pressing Return always opens a new prompt line, including when the handler
  writes no output. The caret previously stayed on the submitted command.
- Output that arrives while the pending input is word-wrapped no longer leaves
  a fragment of that input attached to the output line.
- `Ctrl+Backspace` stops at the prompt instead of deleting through it when the
  prompt ends in a letter or digit.
- `Ctrl+X` with no selection no longer replaces the clipboard with an empty
  string.
- Newlines in pasted text are folded into spaces, so a multi-line paste no
  longer produces lines with no prompt on them.
- `Up` no longer discards a partially typed line: `Down` restores it. Repeating
  the same command no longer stores it twice, so `Up` always moves.
- The `examples/minimal` build no longer compiles the library sources into the
  example, which meant it failed to link on the Qt 5 and Android paths. It
  builds and links the `qsimpleconsole` target instead.

### Changed

- Pasting or dropping outside the input line now inserts at the input instead
  of being ignored, matching how typing behaves and how terminals behave.
- `canPaste()` reports whether the clipboard holds text, rather than whether
  the cursor happens to be after the prompt. A paste is now always valid, so
  the old positional answer no longer described anything. **This is a
  behavioural change to a public method.**
- Shift-modified keys go through normal key handling. `Shift+Return` submits
  the command rather than inserting a bare newline, `Shift+Backspace` cannot
  delete the prompt, and `Shift+Home` selects back to the prompt rather than to
  the start of the line.
- Keyboard shortcuts are matched with `QKeySequence` instead of exact modifier
  comparisons, so each platform's alternative bindings (`Shift+Insert` for
  paste, among others) are handled too.

## [1.0.0]

Initial release.
