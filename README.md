# QSimpleConsole

QSimpleConsole is an easy way to integrate a console with your Qt application.
It is a `QPlainTextEdit` subclass that draws a prompt, keeps everything above
the prompt read-only, remembers a command history, and emits each entered
command as a signal.

![QSimpleConsole](https://i.imgur.com/JJGZ9I1.png)

## Requirements

- Qt 5 or Qt 6 (Widgets)
- C++17
- CMake 3.16 or later

## Installation

Either vendor the repository into your build:

```cmake
add_subdirectory(third_party/QSimpleConsole)

target_link_libraries(myapp PRIVATE QSimpleConsole::qsimpleconsole)
```

Or install it once and find it:

```sh
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
cmake --install build
```

```cmake
find_package(QSimpleConsole 1.1 REQUIRED)

target_link_libraries(myapp PRIVATE QSimpleConsole::qsimpleconsole)
```

The target name is the same either way, and it carries its own include
directory, so `#include "qsimpleconsole.h"` works from anywhere in your
project. You do not need to `find_package(Qt...)` yourself - the package
config pulls in Qt Widgets, which the library exposes publicly.

## Usage

Capture the entered command by connecting to the signal, and write the result
back with `appendOutput`:

```cpp
#include "qsimpleconsole.h"

QSimpleConsole *console = new QSimpleConsole(this);

connect(console, &QSimpleConsole::commandEntered, this, [console](const QString &command) {
    console->appendOutput("Command entered: " + command);
});

setCentralWidget(console);
```

A complete program is in [`examples/minimal`](examples/minimal).

## API

| Member | Description |
| --- | --- |
| `commandEntered(const QString &)` *(signal)* | Emitted when Return is pressed on a non-empty input line. |
| `saveError(const QString &)` *(signal)* | Emitted when `saveToFile()` could not write. Carries the reason. |
| `appendOutput(const QString &)` *(slot)* | Writes a line of output and reopens the prompt below it. |
| `prompt()`, `setPrompt(const QString &)` | The prompt. Defaults to `"> "`. |
| `currentCommand()` | The text currently typed after the prompt. Reading it changes nothing. |
| `setCurrentCommand(const QString &)` | Replaces the text after the prompt, as history navigation does. |
| `setTimestampEnabled(bool)`, `isTimestampEnabled()` | Prefixes each output line with `hh:mm:ss: `. Off by default. |
| `clear()` *(slot)* | Empties the console and draws a fresh prompt. Keeps the history. |
| `clearHistory()` *(slot)* | Drops the recorded commands, so `Up` has nothing to recall. |
| `setScrollbackLines(int)`, `scrollbackLines()` | Caps the output lines kept above the input. 0 keeps every line, and is the default. |
| `canCut()`, `canPaste()` | Whether cut and paste are currently possible. Used for the context menu. |
| `saveToFile(const QString &)` | Writes the console verbatim to a file. Returns `false` and emits `saveError` on failure. |

`prompt`, `timestampEnabled` and `scrollbackLines` are also declared as
`Q_PROPERTY`, so they are reachable from Designer, QML and `setProperty()`.

Output that arrives while a command is half-typed is written above the input,
and the pending text is carried down to the new prompt.

### Keys

| Key | Behaviour |
| --- | --- |
| Return | Submits the input line and emits `commandEntered`. |
| Up, Down | Walk the last 32 commands. Up remembers the line you were typing; Down gives it back. |
| Home, End | Move to the start and end of the input, not of the visual line. |
| Ctrl+Home, Ctrl+End | Move to the top of the scrollback and the end of the input. |
| Ctrl+A | Selects the command, not the whole document. |

Everything above the prompt is read-only. Typing, pasting or dropping text
while the cursor is in the scrollback inserts at the input line instead, and
newlines in pasted text are folded into spaces so the input stays one line.

The right-click menu carries the standard Cut, Copy and Paste entries, with cut
and paste greyed out when they would reach outside the input line, plus
**Save As...**, which asks for a path and calls `saveToFile()`, and **Clear**,
which calls `clear()`.

## Notes

**Threading.** `appendOutput` is a slot and, like all of QtWidgets, must run on
the GUI thread. Producers on other threads should reach it through a queued
signal/slot connection rather than calling it directly.

**Commands are untrusted input.** `commandEntered` forwards whatever was typed
or pasted, unchanged. Validate it before handing it to a shell, an interpreter
or a query engine.

**Entered commands stay in memory.** Every command joins the history and stays
in the document. `clearHistory()` drops the recorded commands and `clear()`
empties the display, but neither overwrites the memory that held them - if the
console is used to enter secrets, treat this as best-effort removal rather than
a secure erase.

**Scrollback is unlimited by default.** Every line of output is kept for the
lifetime of the widget, and Qt holds a laid-out representation of each one -
measured at roughly 4 kB of resident memory per line, so 50,000 lines cost
about 190 MB. A console fed by a long-running producer should cap it:

```cpp
console->setScrollbackLines(5000);
```

Capping the scrollback disables undo/redo, which is how `QPlainTextEdit`'s
underlying block limit behaves.

## Tests

```sh
cmake -B build -DQSIMPLECONSOLE_BUILD_TESTS=ON
cmake --build build
cd build && ctest --output-on-failure
```

The tests drive a real widget but need no display server.

`-DQSIMPLECONSOLE_BUILD_EXAMPLES=ON` builds `examples/minimal` in the same way.

## License

[MIT](https://choosealicense.com/licenses/mit/)
