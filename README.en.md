# Teefax

[🇩🇪 Deutsche Version](README.md)

Teefax is a command-line kitchen timer for Windows. Pass the desired duration or target time as a parameter — when the time is up, an alarm sounds.

It was built as a faster, scriptable alternative to GUI timers: a single command like `teefax 3m` is all it takes to start a 3-minute countdown. Combined with a desktop shortcut, that's one click.

**Latest release: [Download](https://github.com/Alsweider/Teefax/releases/latest)**

<img width="200" height="200" alt="icon" src="https://github.com/user-attachments/assets/22f7878a-98b6-4bac-ba9f-53583990aab2" />

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
- Bring a programme window to the foreground when the timer ends
- Pre-alarm: audible per-second countdown before the final signal
- Stopwatch with centisecond resolution
- Live clock display mode
- Macros: save frequent argument combinations and call them by a short name
- Suppress screensaver and standby during countdown
- Accurate timing using `chrono::steady_clock` — not affected by system load
- Lightweight: ~1.5 MB binary, ~0.7 MB RAM
- Languages: 🇩🇪 🇬🇧 🇫🇷 🇵🇹 🇷🇺 *(Machine-translated from German. [Found an error?](https://github.com/Alsweider/Teefax/issues))*

---

## Installation

1. Download the latest `teefax.exe` from [Releases](https://github.com/Alsweider/Teefax/releases/latest).
2. Place it anywhere you like, e.g. `C:\Tools\Teefax\`.
3. Optionally, add the folder to your system `PATH` so you can call `teefax` from any terminal or the Run dialog (`Win+R`). With PowerShell (no admin rights required, adjust the path):

```powershell
[Environment]::SetEnvironmentVariable("Path", $env:Path + ";C:\Tools\Teefax", "User")
```

Restart your terminal afterwards.

To stop the timer at any time, close the console window or press `Ctrl+C`.

---

## Notes

### Windows SmartScreen and antivirus software

When downloading for the first time, Windows SmartScreen may warn (*"Windows protected your PC"*). To proceed, click **"More info" → "Run anyway"**.

Some antivirus programmes may incorrectly flag Teefax as suspicious. Teefax uses Windows APIs that are also misused by malware, such as launching programmes on a schedule (`--cmd`), opening files (`--open`), and controlling window focus (`--focus`). This is a false positive.

Teefax is open source. The full code is on [GitHub](https://github.com/Alsweider/Teefax) and can be independently verified or compiled from scratch. [VirusTotal](https://www.virustotal.com) can be used for a quick scan.

### Bluetooth headphones

Bluetooth devices enter a power-saving mode after a short period of inactivity. Teefax automatically activates the audio channel shortly before an alarm to ensure reliable playback. For very short timers (a few seconds), the first sound may still be slightly quieter.

Some Bluetooth headphones use automatic volume adjustment (*"loudness compensation"*), which can affect the volume of pre-alarm beeps (i.e. crescendo-decrescendo). In case of problems, it is recommended to disable this setting.

---

## Configuration file

On startup, Teefax reads an optional `teefax.ini` file from the same directory as the `.exe`. The syntax is identical to the command line. One option per line, comments with `#`:

```ini
# teefax.ini
--lang en
--alarm-repeat 3
--nomsg
"C:\Sounds\gong.wav"
```

Command-line arguments override config values (last value wins). A commented example file is available in the repository as [`teefax.ini`](teefax.ini).

---

## Macros

Frequently used argument combinations can be saved as a macro and called by a short name.

```bash
# Save a macro
teefax --macro add tea 3m --msg "Tea is ready!"

# Run the macro
teefax tea

# List all macros
teefax --macro list

# Remove a macro
teefax --macro remove tea
```

Macros are stored as `macro <name> = <arguments>` lines in `teefax.ini`. Names may only contain letters and digits, and must not clash with reserved option names (e.g. `--loop`). When a macro is called, its stored arguments are expanded automatically before all other parameters are evaluated.

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

Combined example: `teefax 1h30m` or `teefax 1h 30m` counts down 1 hour and 30 minutes.

### Options

| Option | Short | Description |
|---|---|---|
| `--mute` | `-m` | No alarm sound |
| `--loop [count]` | `-l` | Repeat the timer (fixed count optional, otherwise infinite) |
| `--for <duration>` | | Stop loop after total elapsed time (requires `--loop`, `--daily` or `--every`) |
| `--at HH:MM[:SS]` | `-a` | Count down to a specific time of day (alias: `--until`) |
| `--at YYYY-MM-DD` | `-a` | Count down to a specific date (midnight) |
| `--at YYYY-MM-DD HH:MM` | `-a` | Count down to a specific date and time |
| `--daily HH:MM[:SS] ...` | `-d` | Daily recurring alarm at one or more times |
| `--every <days> [HH:MM]` | `-e` | Weekly/monthly recurrence (e.g. `mon,fri` or `1,15`) |
| `--alarm-repeat <n>` | `-ar` | Repeat the alarm sound n times after the timer ends |
| `--alarm-interval <s>` | `-ai` | Seconds between repeated alarms (default: 2) |
| `--async` | `-as` | Play alarm sound asynchronously (timer keeps running during playback) |
| `--open <filepath>` | `-o` | Open a file, programme or URL when the timer ends |
| `--cmd <command>` | `-c` | Run a console command when the timer ends |
| `--focus <title>` | `-f` | Bring a window to the foreground when the timer ends (partial title match, case-insensitive) |
| `--prealarm <s>` | `-pa` | Beep every second during the last X seconds |
| `--nomsg` | | Suppress the notification popup |
| `--msg <text>` | | Add a custom note to the notification popup |
| `--time` | `-t` | Live date & time display (clock mode, exit with Ctrl+C) |
| `--nosleep` | `-ns` | Prevent screensaver and standby |
| `--lang <lang>` | `-la` | Set language: `de`, `en`, `fr`, `pt`, `ru` |
| `--version` | `-v` | Show version number |
| `--help` | `-h` | Show help |
| `--stopwatch`| `-sw` | `Space` or `P`: Pause/Resume, `Ctrl+C`: Exit |
| `--macro <list\|add\|remove>` | | Manage macros (see [Macros](#macros)) |

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
teefax 10s --loop --for 2m  # repeat until 2 minutes total have elapsed

# Silent timer (no sound, no popup)
teefax 20s --mute --nomsg

# Custom note in the notification popup
teefax 5m --msg "Tea is ready!"

# Open a file when done
teefax 5m --open "C:\Notes\todo.txt"

# Run a command when done
teefax 20s --cmd "shutdown /s /t 0"

# Bring a window to the foreground when done
teefax 5m --focus "Notepad"

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

# Save a macro and run it
teefax --macro add tea 3m --msg "Tea is ready!"
teefax tea
```

---

## How it works

Teefax measures time intervals using [`chrono::steady_clock`](https://cplusplus.com/reference/chrono/steady_clock/) from the C++11 standard library. This clock runs monotonically, is independent of the system time, and is not affected by system load. Instead of relying on a simple `Sleep()` call, Teefax always calculates the exact target time and waits with [`this_thread::sleep_until()`](https://cplusplus.com/reference/thread/this_thread/sleep_until/) precisely until that moment. For absolute points in time (`--at`, `--daily`, `--every`), the wall clock ([`system_clock`](https://cplusplus.com/reference/chrono/system_clock/)) is used so that calendar dates and times are evaluated correctly. To ensure sleep accuracy on Windows, the system timer resolution is set to 1 ms for the duration of the programme (`timeBeginPeriod`), since the default Windows resolution of approximately 15.6 ms would otherwise limit the precision of `sleep_until()`.

**Precision per mode**

In countdown mode (`teefax 5m`, `1h30m`, etc.), time is measured using `steady_clock`, which is driven by the processor's hardware oscillator. This oscillator is subject to a small crystal drift, typically 10–50 ppm (parts per million). For short to medium timers this is negligible: a 5-minute timer deviates by less than 0.015 seconds. For very long timers the drift accumulates: a 24-hour timer may deviate by 1-4 seconds. However, `steady_clock` runs monotonically and is completely immune to system intervention. NTP corrections, daylight saving transitions, or manual clock changes cannot disturb the countdown.

`--at`, `--daily` and `--every` use `system_clock`, which is continuously synchronised with time servers via NTP. The only remaining inaccuracy is OS scheduler jitter at the final wakeup. Typically a few milliseconds, regardless of timer duration. In return, system time changes can in principle affect these modes.

For timers spanning several hours or days where the end time should match wall-clock time precisely, `--at` is the more accurate choice.

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