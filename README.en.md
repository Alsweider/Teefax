# Teefax

[🇩🇪 Deutsche Version](README.md)

Teefax is a command-line kitchen timer for Windows. Pass the desired duration or target time as a parameter — when the time is up, an alarm sounds.

It was built as a faster, scriptable alternative to GUI timers: a single command like `teefax 3m` is all it takes to start a 3-minute countdown. Combined with a desktop shortcut, that's one click.

**Latest release: [Download](https://github.com/Alsweider/Teefax/releases/latest)**

![teefax3](https://github.com/user-attachments/assets/b923ca54-9185-4cb9-a3c7-5686e9508086)
![teefax4](https://github.com/user-attachments/assets/f7bb81db-b6c0-45da-822e-8e5a7bac5682)
![teefaxtime](https://github.com/user-attachments/assets/8ac423dd-181c-4c67-b0b6-a614adb2bc56)

## Features

- Start a timer with a single click via desktop shortcut
- Count down by duration or to a specific time or date
- Custom alarm sound (.WAV format)
- Daily recurring alarms
- Loop / repeat timers
- Open a file or run a console command when the timer ends
- Pre-alarm: audible per-second countdown before the final signal
- Live clock display mode
- Suppress screensaver and standby during countdown
- Accurate timing using `chrono::steady_clock` — not affected by system load
- Lightweight: ~150 KB binary, ~0.7 MB RAM
- Languages: 🇩🇪 🇬🇧 🇫🇷 🇵🇹 🇷🇺

---

## Installation

1. Download the latest `teefax.exe` from [Releases](https://github.com/Alsweider/Teefax/releases/latest).
2. Place it anywhere you like, e.g. `C:\Tools\Teefax\`.
3. Optionally, add the folder to your system `PATH` so you can call `teefax` from any terminal or the Run dialog (`Win+R`).

To stop the timer at any time, close the console window or press `Ctrl+C`.

---

## Usage

```
teefax [<time>] [soundfile] [options]
```

### Time formats

Units are written directly after the number, without spaces. Units can be combined freely.

| Unit | Syntax |
|---|---|
| Milliseconds | `ms` |
| Seconds | `30` or `30s` or `30sec` |
| Minutes | `5m` or `5min` |
| Hours | `2h` or `2hr` |
| Days | `1d` or `1day` |
| Weeks | `1w` or `1wk` or `1week` |
| Months (30 days) | `1mo` or `1mon` or `1month` |
| Years (365 days) | `1y` or `1yr` or `1year` |

Combined example: `teefax 1h30m` counts down 1 hour and 30 minutes.

### Options

| Option | Short | Description |
|---|---|---|
| `--mute` | `-m` | No alarm sound |
| `--loop [count]` | `-l` | Repeat the timer (fixed count optional, otherwise infinite) |
| `--at HH:MM[:SS]` | `-a` | Count down to a specific time of day |
| `--at YYYY-MM-DD` | `-a` | Count down to a specific date (midnight) |
| `--at YYYY-MM-DD HH:MM` | `-a` | Count down to a specific date and time |
| `--daily HH:MM[:SS] ...` | `-d` | Daily recurring alarm at one or more times |
| `--alarm-repeat <n>` | `-ar` | Repeat the alarm sound n times after the timer ends |
| `--alarm-interval <s>` | `-ai` | Seconds between repeated alarms (default: 2) |
| `--async` | `-as` | Play alarm sound asynchronously (timer keeps running during playback) |
| `--open <filepath>` | `-o` | Open a file or program when the timer ends |
| `--cmd <command>` | `-c` | Run a console command when the timer ends |
| `--prealarm <s>` | `-pa` | Beep every second during the last X seconds |
| `--time` | `-t` | Live date & time display (clock mode, exit with Ctrl+C) |
| `--nosleep` | `-ns` | Prevent screensaver and standby |
| `--nomsg` | | Suppress the notification popup |
| `--lang <lang>` | `-la` | Set language: `de`, `en`, `fr`, `pt`, `ru` |

---

## Examples

```bash
# Simple timers
teefax 180              # 3 minutes (plain number = seconds)
teefax 5m               # 5 minutes
teefax 1h30m            # 1 hour 30 minutes
teefax 2m30s            # 2 minutes 30 seconds

# Count down to a specific time or date
teefax --at 07:30
teefax --at 07:30:15
teefax --at 2030-12-31
teefax --at 2030-12-31 20:00

# Daily recurring alarm
teefax --daily 8:00 13:00 18:00

# Custom alarm sound
teefax 5m "C:\Sounds\gong.wav"

# Loop
teefax 10s --loop           # repeat forever
teefax 3m --loop 5          # repeat 5 times

# Silent timer (no sound, no popup)
teefax 20s --mute --nomsg

# Open a file when done
teefax 5m --open "C:\Notes\todo.txt"

# Run a command when done
teefax 20s --cmd "shutdown /s /t 0"

# Pre-alarm: beep every second for the last 5 seconds
teefax 30s --prealarm 5

# Live clock display
teefax --time

# Suppress standby during a long timer
teefax 2h --nosleep

# Recursive: 5-second countdown with beeps, then start a 20-second timer
teefax 5s --prealarm 5 --nomsg --cmd "start teefax 20s --prealarm 5"

# Set language explicitly
teefax 5m --lang en
```

---

## How it works

Teefax uses [`chrono::steady_clock`](https://cplusplus.com/reference/chrono/steady_clock/) from the C++11 standard library to measure time. This clock is monotonic and independent of the system clock — it never drifts backwards and is unaffected by system load. Rather than using a simple `Sleep()` call (which accumulates milliseconds of error over many iterations), Teefax always calculates the precise target timestamp and waits until that exact moment using `this_thread::sleep_until()`.

---

## Building from source

Requires a C++17 compiler and the Windows Multimedia API (`winmm`).

**With Qt Creator / qmake:**
```bash
qmake Teefax.pro
make
```

**With MinGW directly:**
```bash
g++ -std=c++17 -o teefax main.cpp -lwinmm
```

---
