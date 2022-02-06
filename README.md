```
██████╗ ██╗   ██╗████████╗███████╗███╗   ██╗██╗   ██╗████████╗███████╗
██╔══██╗╚██╗ ██╔╝╚══██╔══╝██╔════╝████╗  ██║██║   ██║╚══██╔══╝██╔════╝
██████╔╝ ╚████╔╝    ██║   █████╗  ██╔██╗ ██║██║   ██║   ██║   ███████╗
██╔══██╗  ╚██╔╝     ██║   ██╔══╝  ██║╚██╗██║██║   ██║   ██║   ╚════██║
██████╔╝   ██║      ██║   ███████╗██║ ╚████║╚██████╔╝   ██║   ███████║
╚═════╝    ╚═╝      ╚═╝   ╚══════╝╚═╝  ╚═══╝ ╚═════╝    ╚═╝   ╚══════╝
```

Bytenuts aims to be a light weight terminal replacement of GUI based serial communications apps like SecureCRT or ZOC. It allows for simple communication between the PC and serial device.

Bytenuts has been tested and built on x86-64 and 32-bit ARM Ubuntu 18.

## Features

- Input queueing - The input line allows users to edit their command before sending the characters over the serial connection
- XModem transfers - Users can begin an XModem transfer of a file on disc
- 8-bit ANSI color support - 8-bit terminal color can be enabled
- Configuration - Bytenuts can be configured with the config file located at `~/.config/bytenuts/config`
- Input echoing - Bytenuts can echo user input rather than relying on the connected device to echo
- Command history - Simply press up/down arrow to load previous commands
- Output history - Use page up/down, home/end, and ctrl + up/down arrow to scroll through the output window
- Output logging - Output can be saved to a log file passed in with the `-l` option
- Quick commands - Pages of quick commands are loaded from `~/.config/bytenuts/commands[1-10]`

## Planned Features

Bytenuts is still very early in development. Here are some features that may get implemented.

- Other transfer protocols - Maybe Kermit and ZModem

## Un-Planned Features

Bytenuts is not supposed to be the be-all end-all application, nor is it supposed to be able to do everything SecureCRT can. Here are some things it probably won't have.

- Non-serial communications - No SSH, telnet, etc.
- Elaborate menuing - Bytenuts functions will be shortcut driven to focus on what's important (the serial communication)
- Script loading - Think like how you can load scripts in ZOC on SecureCRT. Write normal Python scripts if you want to do that kind of stuff.
- Multi connection support - Open another instance of Bytenuts if you need to
- Windows support - No

## Launch Options

Launching Bytenuts like `bytenuts -h` provides this help print:

```
USAGE

bytenuts [OPTIONS] <serial path>

Configs get loaded from ${HOME}/.bytenuts/config (if file exists)

OPTIONS

-h: show this help
-b <baud>: set a baud rate (default 115200)
-l <path>: log all output to the given file
-c <path>: load a config from the given path rather than the default
```

## Navigation

Controls like backspace, delete, end/home, and left/right arrow work as expected within the input buffer window. For the output window, you can use the following keys to scroll through it:

- `ctrl + up/down arrow` - Scroll the output window up or down by one line
- `page up/down` - Scroll the output window up or down by half of the height of the output window
- `shift + home/end` - Jump to the beginning or the end of the output

If the output window reaches the current line of output, then the output will continue scrolling in real time. Otherwise, the output is paused to continue viewing where you currently are.

## Configuration File

There are only three supported configs currently. Each config is defined like `<name>=<value>`. Here is a sample config:

```
colors=1
echo=0
no_crlf=0
```

- `colors` - enable parsing of 8-bit ANSI color codes
- `echo` - echo input to the terminal in app
- `no_crlf` - just send a line feed (`\n`) for user input rather than carriage return + line feed (`\r\n`)

Bytenuts looks for the configs at `~/.config/bytenuts/config`.

## Commands

Upon starting bytenuts you get this prompt:

```
Welcome to Bytenuts
To quit press ctrl-b q
To see all commands, press ctrl-b h
```

The help prompt lists the available commands.

```
Commands (lead with ctrl-b):
  c: print available quick commands
  0-9: load the given quick command (0 is 10)
  p, 0-9: select a different quick commands page (0 is 10)
  i: view info/stats
  x: start XModem upload with 128B payloads
  X: start XModem upload with 1024B payloads
  h: view this help
  q: quit Bytenuts
```

## Quick Commands

You can provide multiple pages of quick commands you can easily load in with `ctrl+b [0-9]` in the files `~/.config/bytenuts/commands<idx>`. Bytenuts will load pages starting from `commands1` until a `commands<idx>` no longer exists. The commands should be newline separated. When a command is loaded, the entire contents of the line is loaded into the input buffer.

Bytenuts will only store the first ten commands for each page, so the first 10 lines of your command files will be used.

On startup, command page 1 is selected. You can select which command page is currently loaded by doing `ctrl+p, [0-9]` to load the desired page (note that index 0 will correspond to `commands10`).

## Bugs

Check out known bugs in the [issues tab](https://github.com/cookthebook/bytenuts/issues?q=is%3Aissue+is%3Aopen+label%3Abug).

## Building

Building Bytenuts is very simple. All you need is clang and libncurses. Run `make` in the Bytenuts root directory to build. You can also install the build (creating a link in `/usr/local/bin` to the `build` directory) by running `sudo make install`.
