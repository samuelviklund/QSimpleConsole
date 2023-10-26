# QSimpleConsole

QSimpleConsole is an easy way to integrate a console with your Qt application.
![QSimpleConsole](https://i.imgur.com/JJGZ9I1.png)

## Installation

Include the files in your project

## Usage

Capture the entered command by connecting to the signal
```cpp

QSimpleWidget *widget = new QSimpleWidget();

connect(console, &QSimpleConsole::consoleCommand, this, [console](QString command) {
    console->writeOutput("Command entered: " + command);
});
```

## License

[MIT](https://choosealicense.com/licenses/mit/)