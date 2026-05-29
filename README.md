# Teefax
[🇬🇧 English version](README.en.md)

Teefax ist ein Kommandozeilen-Küchenwecker für Windows. Als Parameter erwartet er die gewünschte Wartezeit oder Uhrzeit. Nach Ablauf ertönt ein Signal.

**Neueste Version: [Herunterladen](https://github.com/Alsweider/Teefax/releases/latest)**

<img width="200" height="200" alt="icon" src="https://github.com/user-attachments/assets/7cd1ff05-8dfc-4c0c-a899-534fc7526802" />

Ich habe lange [Timerle](https://www.jfsoftware.de/timerle.htm) von JFSoftware verwendet, um Tee zu kochen. Timerle wurde seit 2006 nicht mehr vom Entwickler aktualisiert, aber funktioniert unverändert und ist ein ausreichend präziser GUI-Timer für den Alltag. Für meine Bedürfnisse war der Funktionsumfang allerdings noch verbesserungswürdig und auch die Genauigkeit der Zeitlogik lässt sich mit aktuellen Mitteln besser umsetzen. Darum ist Teefax als schnelle, skriptfähige Alternative zu GUI-Timern entstanden. Ein einziger Befehl wie `teefax 3m` startet sofort einen 3-Minuten-Zähler. Mit einer Desktop-Verknüpfung reicht schon ein Klick. 

![teefax3](https://github.com/user-attachments/assets/b923ca54-9185-4cb9-a3c7-5686e9508086)
![teefax4](https://github.com/user-attachments/assets/f7bb81db-b6c0-45da-822e-8e5a7bac5682)
![teefaxtime](https://github.com/user-attachments/assets/8ac423dd-181c-4c67-b0b6-a614adb2bc56)

## Funktionen

- Zähler per Desktop-Verknüpfung mit einem Klick starten
- Nach Dauer oder bis zu einer bestimmten Uhrzeit bzw. einem Datum zählen
- Beliebigen Weckton festlegen (.WAV-Format)
- Schleifen- / Wiederholungsfunktion und tägliche Wiederholalarme
- Datei öffnen oder Konsolenbefehl ausführen nach Ablauf
- Programmfenster nach Ablauf in den Vordergrund holen
- Voralarm: sekündliches Piepsen vor dem Schlussalarm
- Stoppuhr mit Zentisekundenauflösung
- Mini-Uhr: Direktanzeige von Datum und Uhrzeit
- Makros: häufige Befehlskombinationen speichern und mit Kurzname aufrufen
- Bildschirmschoner und Standby während des Countdowns unterdrücken
- Präzise Zeitmessung mit `chrono::steady_clock`, unabhängig von der Systemauslastung
- Klein: ~200 KB Programmdatei, ~0,7 MB RAM
- Sprachen: 🇩🇪 🇫🇷 🇵🇹 🇬🇧 🇷🇺 *(Maschinenübersetzt aus dem Deutschen. [Fehler gefunden?](https://github.com/Alsweider/Teefax/issues))*

---

## Installation

1. Die aktuelle `teefax.exe` von [Releases](https://github.com/Alsweider/Teefax/releases/latest) herunterladen.
2. Die Datei an einem beliebigen Ort ablegen, z. B. `C:\Tools\Teefax\`.
3. Optional: Den Ordner als `PATH`-Eintrag in den Systemvariablen ergänzen. Danach ist `teefax` direkt in der Kommandozeile und im Ausführen-Dialog (`Win+R`) verfügbar, ohne zum Ordner navigieren zu müssen. Wie bei allen CLI-Programmen ist auch das Ansprechen über eine Stapelverarbeitungsdatei (.bat) oder das Anlegen einer Verknüpfung mit den gewünschten Parametern möglich. 

Den Timer jederzeit beenden: Konsolenfenster schließen oder `Strg`+`C` drücken.

---

## Konfigurationsdatei

Teefax liest beim Start eine optionale Konfigurationsdatei `teefax.ini` aus demselben Verzeichnis wie die `.exe`. Die Syntax ist identisch mit der Kommandozeile. Eine Option pro Zeile, Kommentare mit `#`:

```ini
# teefax.ini
--lang en
--alarm-repeat 3
--nomsg
"C:\Sounds\gong.wav"
```

Kommandozeilenargumente überschreiben Config-Werte (letzter Wert gewinnt). Eine kommentierte Beispieldatei liegt im Repository unter [`teefax.ini`](teefax.ini).

---

## Makros

Häufig genutzte Befehlskombinationen lassen sich als Makro speichern und danach mit einem kurzen Namen aufrufen.

```bash
# Makro anlegen
teefax --macro add tee 3m --msg "Tee fertig!"

# Makro aufrufen
teefax tee

# Alle Makros anzeigen
teefax --macro list

# Makro entfernen
teefax --macro remove tee
```

Makros werden als `macro <Name> = <Argumente>`-Zeilen in der `teefax.ini` gespeichert. Makronamen dürfen nur Buchstaben und Ziffern enthalten und dürfen keine reservierten Optionsnamen (z. B. `--loop`) sein. Beim Aufruf werden die gespeicherten Argumente automatisch expandiert, bevor alle weiteren Parameter ausgewertet werden.

---

## Nutzung

```
teefax [<Zeit>] [Sounddatei] [Optionen]
```

### Zeitformate

Einheiten werden direkt ohne Leerzeichen an die Zahl geschrieben und lassen sich frei kombinieren.

| Einheit | Syntax |
|---|---|
| Millisekunden | `ms` |
| Sekunden | `30` oder `30s` oder `30sec` |
| Minuten | `5m` oder `5min` |
| Stunden | `2h` oder `2hr` |
| Tage | `1d` oder `1day` |
| Wochen | `1w` oder `1wk` oder `1week` |
| Monate (30 Tage) | `1mo` oder `1mon` oder `1month` |
| Jahre (365 Tage) | `1y` oder `1yr` oder `1year` |

Kombiniertes Beispiel: `teefax 1h30m` oder `teefax 1h 30m` zählt 1 Stunde und 30 Minuten.

### Optionen

| Option | Kurz | Beschreibung |
|---|---|---|
| `--mute` | `-m` | Kein Weckton |
| `--loop [Anzahl]` | `-l` | Timer wiederholen (feste Anzahl optional, sonst unbegrenzt) |
| `--at HH:MM[:SS]` | `-a` | Bis zu einer bestimmten Uhrzeit zählen (Alias: `--until`) |
| `--at YYYY-MM-DD` | `-a` | Bis zu einem bestimmten Datum zählen (Mitternacht) |
| `--at YYYY-MM-DD HH:MM` | `-a` | Datum und Uhrzeit kombiniert |
| `--daily HH:MM[:SS] ...` | `-d` | Täglicher Alarm zu einer oder mehreren Uhrzeiten |
| `--every <Tage> [HH:MM]` | `-e` | Wöchentliche/monatliche Wiederholung (z. B. `mon,fri` oder `1,15`) |
| `--alarm-repeat <n>` | `-ar` | Weckton nach Ablauf n-mal wiederholen (Standard: 1) |
| `--alarm-interval <s>` | `-ai` | Sekunden zwischen den Wiederholungen (Standard: 2) |
| `--async` | `-as` | Weckton asynchron abspielen (Timer läuft während Wiedergabe weiter) |
| `--open <Dateipfad>` | `-o` | Datei, Programm oder URL nach Ablauf öffnen |
| `--cmd <Befehl>` | `-c` | Konsolenbefehl nach Ablauf ausführen |
| `--focus <Titel>` | `-f` | Fenster nach Ablauf in den Vordergrund holen (Teiltitel, Groß-/Kleinschreibung egal) |
| `--prealarm <s>` | `-pa` | X Sekunden vor Ablauf sekündlich piepsen |
| `--nomsg` | | Benachrichtigungsfenster unterdrücken |
| `--msg <Text>` | | Eigene Notiz im Benachrichtigungsfenster |
| `--time` | `-t` | Direktanzeige von Datum und Uhrzeit (Beenden mit Strg+C) |
| `--nosleep` | `-ns` | Bildschirmschoner und Standby unterdrücken |
| `--lang <Sprache>` | `-la` | Sprache festlegen: `de`, `en`, `fr`, `pt`, `ru` |
| `--version` | `-v` | Versionsnummer anzeigen |
| `--help` | `-h` | Hilfe anzeigen |
| `--stopwatch`| `-sw` | Stoppuhr starten (`Leertaste` oder `P`: Pause/Weiter, `Strg+C`: Beenden) |

---

## Beispiele

```bash
# Einfache Timer
teefax 180              # 3 Minuten (reine Zahl = Sekunden)
teefax 5m               # 5 Minuten
teefax 1h30m            # 1 Stunde 30 Minuten
teefax 2m30s            # 2 Minuten 30 Sekunden

# Bis zu einer bestimmten Uhrzeit oder einem Datum
teefax --at 07:30
teefax --at 07:30:15
teefax --at 2030-12-31
teefax --at 2030-12-31 20:00

# Täglicher Wiederholalarm
teefax --daily 8:00 13:00 18:00

# Eigener Weckton
teefax 5m "C:\Klänge\gong.wav"

# Schleifen
teefax 10s --loop           # unbegrenzt wiederholen
teefax 3m --loop 5          # 5-mal wiederholen

# Stiller Timer (kein Ton, kein Popup)
teefax 20s --mute --nomsg

# Eigene Notiz im Benachrichtigungsfenster
teefax 5m --msg "Tee fertig!"

# Datei nach Ablauf öffnen
teefax 5m --open "C:\Notizen\todo.txt"

# Befehl nach Ablauf ausführen
teefax 20s --cmd "shutdown /s /t 0"

# Fenster nach Ablauf in den Vordergrund holen
teefax 5m --focus "Notepad"

# Voralarm: letzte 5 Sekunden sekündlich piepsen
teefax 30s --prealarm 5

# Mini-Uhr
teefax --time

# Standby während eines langen Timers unterdrücken
teefax 2h --nosleep

# Rekursiv: 5-Sekunden-Startzähler mit Piepsen, danach 20-Sekunden-Timer
teefax 5s --prealarm 5 --nomsg --cmd "start teefax 20s --prealarm 5"

# Sprache explizit setzen
teefax 5m --lang de

# Makro anlegen und aufrufen
teefax --macro add tee 3m --msg "Tee fertig!"
teefax tee
```

---

## Funktionsweise & Technik

Teefax misst Zeitintervalle mit [`chrono::steady_clock`](https://cplusplus.com/reference/chrono/steady_clock/) aus der C++11-Standardbibliothek. Diese Uhr läuft monoton, ist unabhängig von der Systemzeit und wird durch Systemauslastung nicht beeinflusst. Anstatt eines einfachen `Sleep()`-Aufrufs errechnet Teefax stets den genauen Zielzeitpunkt und wartet mit [`this_thread::sleep_until()`](https://cplusplus.com/reference/thread/this_thread/sleep_until/) exakt bis zu diesem Moment. Für absolute Zeitpunkte (`--at`, `--daily`, `--every`) wird die Wanduhr ([`system_clock`](https://cplusplus.com/reference/chrono/system_clock/)) verwendet, damit Kalender- und Uhrzeitangaben korrekt ausgewertet werden. Um die Schlafgenauigkeit auf Windows sicherzustellen, wird die Systemtimerauflösung für die Laufzeit des Programms auf 1 ms gesetzt (`timeBeginPeriod`), da die Windows-Standardauflösung von ~15,6 ms sonst die Präzision von `sleep_until()` begrenzen würde.

**Genauigkeit der Modi**

Im Countdown-Modus (`teefax 5m`, `1h30m` usw.) basiert die Zeitmessung auf `steady_clock`, die intern vom Hardwaretakt des Prozessors angetrieben wird. Dieser unterliegt einer geringen Quarzoszillatordrift von typischerweise 10-50 ppm (Millionstel). Für kurze bis mittlere Timer ist das vernachlässigbar, da ein 5-Minuten-Timer weniger als 0,015 Sekunden abweicht. Bei sehr langen Timern summiert sich die Drift: Ein 24-Stunden-Timer kann bereits 1-4 Sekunden abweichen. Dafür läuft `steady_clock` monoton und ist vollständig immun gegen Systemeingriffe. NTP-Korrekturen, Sommer-/Winterzeitumstellungen oder manuelle Uhrzeitänderungen können den Zähler nicht stören.

`--at`, `--daily` und `--every` verwenden `system_clock`, die meist per NTP laufend mit Zeitservern synchronisiert wird. Die einzige verbleibende Ungenauigkeit ist der OS-Scheduler-Jitter beim letzten Aufwachen, der meist wenige Millisekunden beträgt, unabhängig von der Timer-Dauer. Dafür können Systemzeiteingriffe den Ablauf im Prinzip beeinflussen.

Für Zähler über mehrere Stunden oder Tage, bei denen die Endzeit exakt mit der Wanduhrzeit übereinstimmen soll, ist `--at` in der Regel die präzisere Wahl.

---

## Aus dem Quellcode bauen

Benötigt wird ein C++17-Compiler sowie die Windows Multimedia API (`winmm`).

**Mit Qt Creator / qmake:**
```bash
qmake Teefax.pro
make
```

**Direkt mit MinGW:**
```bash
g++ -std=c++17 -o teefax main.cpp -lwinmm
```

---