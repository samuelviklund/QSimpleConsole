# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
- A slot connected to `commandEntered` can now change console state - writing
  output, pre-filling the next command - without having its work overwritten
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
  comparisons, so each platform's alternative bindings - `Shift+Insert` for
  paste, among others - are handled too.

## [1.0.0]

Initial release.
