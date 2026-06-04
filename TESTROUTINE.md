# Teefax - Testroutine

Vor jedem Release vollständig durchführen. Automatisierte Tests zuerst,
dann manuelle Checks in der Reihenfolge der Liste.

---

## 1. Vorbereitung

- [ ] `teefax.exe` frisch kompiliert (Release-Build, kein Debug)
- [ ] `teefax.ini` im selben Ordner leer oder nicht vorhanden
- [ ] Lautsprecher / Kopfhörer aktiv und Lautstärke hörbar eingestellt
- [ ] Kein anderes `teefax.exe` im Hintergrund aktiv

---

## 2. Automatisierte Smoke-Tests

```
test_teefax.bat
```

- [ ] Alle Tests bestanden (0 Fehlschläge)

Bei Fehlschlägen: Ursache ermitteln, beheben, Skript erneut ausführen, bevor weitergemacht wird.

---

## 3. Anzeige & Ausgabe

### 3.1 Startmeldung und Fortschrittsbalken

```
teefax 5s
```

- [ ] Startmeldung erscheint mit korrekter Versionsnummer
- [ ] Countdown-Anzeige (`Remaining: 5s`) erscheint
- [ ] Zielzeit wird in der Startmeldung angezeigt (z. B. `(bis 14:23:07)`)
- [ ] Fortschrittsbalken füllt sich von links nach rechts
- [ ] Kein Flackern, keine Zeichenmüll-Ausgabe
- [ ] Fenster-Titelleiste zeigt verbleibende Zeit

### 3.2 Abschlussanzeige

- [ ] Balken zeigt am Ende `[##############################]` (vollständig gefüllt)
- [ ] Abschlussmeldung (`Timer finished.` o. ä.) erscheint auf einer neuen Zeile
- [ ] Kein Zeichenmüll zwischen Balken und Abschlussmeldung

### 3.3 Zeitformate in der Ausgabe

```
teefax 1h30m --mute --nomsg
```

- [ ] Startmeldung zeigt `1h 30m` (oder äquivalent)
- [ ] Countdown zählt korrekt herunter

```
teefax 2m30s --mute --nomsg
```

- [ ] Kombination aus Minuten und Sekunden korrekt angezeigt

### 3.4 Fenstertitel mit --msg

```
teefax 10s --mute --nomsg --msg "Tee fertig"
```

- [ ] Fenstertitel zeigt verbleibende Zeit + Auszug der Notiz

---

## 4. Ton (Sound)

### 4.1 Eingebetteter Ton

```
teefax 2s
```

- [ ] Alarm ertönt nach Ablauf (eingebetteter Ton)
- [ ] Benachrichtigungsfenster erscheint
- [ ] Ton ist sauber, kein Knacken oder Abbruch

### 4.2 Benutzerdefinierter Ton

```
teefax 2s "C:\Windows\Media\chimes.wav"
```

- [ ] Externe WAV-Datei wird abgespielt

### 4.3 Alarm-Wiederholung

```
teefax 2s --alarm-repeat 3 --alarm-interval 1
```

- [ ] Alarm ertönt 3-mal mit je ~1 Sekunde Abstand

```
teefax 2s --alarm-repeat abc
```

- [ ] Warnung auf `stderr` (`invalid --alarm-repeat value`), Alarm ertönt einmal (Fallback: 1)

```
teefax 2s --alarm-repeat -5
```

- [ ] Warnung auf `stderr`, Alarm ertönt einmal (Fallback: 1)

### 4.4 Kein Ton

```
teefax 2s --mute
```

- [ ] Kein Ton, Benachrichtigungsfenster erscheint trotzdem

### 4.5 Asynchroner Ton

```
teefax 2s --async
```

- [ ] Timer endet, Ton startet (Programm beendet sich ggf. vor Ende des Tons, Ton und MessageBox gleichzeitig, OK)

---

## 5. Benachrichtigungsfenster

```
teefax 2s --mute
```

- [ ] MessageBox erscheint nach Ablauf
- [ ] Titel lautet `Teefax`
- [ ] Text enthält die Standardmeldung
- [ ] Fenster erscheint im Vordergrund

```
teefax 2s --mute --msg "Tee ist fertig!"
```

- [ ] Benutzerdefinierter Text erscheint in der MessageBox

```
teefax 2s --mute --nomsg
```

- [ ] Kein Benachrichtigungsfenster

---

## 6. Zielzeit (--at)

```
teefax --at HH:MM    (ca. 1-2 Minuten in der Zukunft)
```

- [ ] Countdown läuft bis zur angegebenen Uhrzeit
- [ ] Startmeldung zeigt korrekte Zielzeit

```
teefax --at HH:MM    (Uhrzeit liegt in der Vergangenheit → morgen)
```

- [ ] Suffix `(morgen)` / `(tomorrow)` erscheint in der Ausgabe

```
teefax --at 1999-01-01
```

- [ ] Fehlermeldung, Exit-Code 1 (vergangenes Datum)

---

## 7. Schleife (--loop)

```
teefax 2s --loop 3 --mute --nomsg
```

- [ ] Timer läuft exakt 3-mal durch
- [ ] Durchlaufzähler (`Loop 1`, `Loop 2`, `Loop 3`) wird angezeigt
- [ ] Nach dem 3. Durchlauf: Abschlussmeldung, Programm beendet sich

```
teefax 2s --loop --mute --nomsg
```

- [ ] Timer läuft unbegrenzt
- [ ] `Strg+C` beendet das Programm sauber (kein hängender Prozess)

---

## 8. Voralarm (--prealarm)

```
teefax 10s --prealarm 5 --mute --nomsg
```

- [ ] In den letzten 5 Sekunden ertönt jede Sekunde ein kurzer Beep
- [ ] Beep auch über Bluetooth-Kopfhörer hörbar *(bekanntes Problem, ggf. Hinweis dokumentieren)*

---

## 9. Täglicher Alarm (--daily)

```
teefax --daily HH:MM    (ca. 1 Minute in der Zukunft)
```

- [ ] Countdown läuft bis zur angegebenen Uhrzeit
- [ ] Nach Ablauf: Timer wartet automatisch auf den nächsten Zeitpunkt (loop)
- [ ] `Strg+C` beendet sauber

---

## 10. Wiederholender Alarm (--every)

```
teefax --every mon,fri 08:00    (nächsten passenden Wochentag abwarten oder kurz in Zukunft setzen)
```

- [ ] Startmeldung zeigt Wochentage und Uhrzeit
- [ ] Countdown läuft bis zum nächsten passenden Zeitpunkt
- [ ] Nach Ablauf: Timer wartet automatisch auf den übernächsten Termin (loop)
- [ ] `Strg+C` beendet sauber

```
teefax --every 1,15 09:00    (1. und 15. des Monats)
```

- [ ] Startmeldung zeigt Monatstage und Uhrzeit

```
teefax --every xyz
```

- [ ] Fehlermeldung, Exit-Code 1

```
teefax --every mon,1
```

- [ ] Fehlermeldung (gemischte Wochentag- und Monatstag-Angabe), Exit-Code 1

---

## 11. Aktionen nach Ablauf

### 11.1 Datei öffnen

```
teefax 2s --mute --nomsg --open "C:\Windows\notepad.exe"
```

- [ ] Notepad öffnet sich nach Ablauf

```
teefax 2s --mute --nomsg --open "C:\___nx___\datei.txt"
```

- [ ] Fehlermeldung (Datei nicht gefunden), kein Absturz, Exit 0

### 11.2 Konsolenbefehl

```
teefax 2s --mute --nomsg --cmd "echo teefax_cmd_test_ok"
```

- [ ] Befehl wird ausgeführt, Ausgabe erscheint in der Konsole

```
teefax 2s --mute --nomsg --cmd "notepad.exe"
```

- [ ] Notepad öffnet sich nach Ablauf *(Programm wartet auf Schließen von Notepad erwartet)*

### 11.3 Fensterfokus (--focus)

```
:: Notepad öffnen, dann:
teefax 5s --mute --nomsg --focus "Editor"
```

- [ ] Warnung `Warnung: Fenster ... nicht gefunden` erscheint **nicht** (Fenster ist offen)
- [ ] Nach Ablauf: Notepad-Fenster erscheint im Vordergrund

```
teefax 5s --mute --nomsg --focus "___nichtvorhanden___"
```

- [ ] Warnung `Warnung: Fenster ... nicht gefunden` erscheint beim Start
- [ ] Nach Ablauf: Fehlermeldung, Loop (falls aktiv) wird gestoppt

### 11.4 Aktionen im Loop

```
teefax 2s --loop 3 --mute --nomsg --cmd "echo loop_cmd"
```

- [ ] Befehl wird nach jedem Durchlauf ausgeführt (3-mal)
- [ ] Ausgaben der einzelnen Durchläufe erscheinen sauber untereinander, kein Zeichenmüll

---

## 12. Interaktive Modi

### 12.1 Uhrzeitanzeige

```
teefax --time
```

- [ ] Datum und Uhrzeit werden angezeigt und aktualisieren sich jede Sekunde
- [ ] Datum und Uhrzeit sind auch im Fenstertitel sichtbar
- [ ] `Strg+C` beendet sauber

### 12.2 Stoppuhr

```
teefax --stopwatch
```

- [ ] Stoppuhr läuft (Zentisekunden sichtbar)
- [ ] `Leertaste` oder `P` pausiert, `[PAUSED]` erscheint in der Anzeige
- [ ] Erneute `Leertaste` setzt fort, Zeit stimmt weiter
- [ ] `R` setzt die Stoppuhr auf 0 zurück und pausiert sie
- [ ] Nach Reset: erneute `Leertaste` startet neu
- [ ] `Strg+C` beendet sauber

---

## 13. Sprachversionen

Jeweils prüfen, ob Startmeldung in der richtigen Sprache erscheint:

```
teefax 2s --mute --nomsg --lang en
teefax 2s --mute --nomsg --lang de
teefax 2s --mute --nomsg --lang fr
teefax 2s --mute --nomsg --lang pt
teefax 2s --mute --nomsg --lang ru
```

- [ ] EN: `Teefax [v...] started`
- [ ] DE: `Teefax [v...] gestartet`
- [ ] FR: `Teefax [v...] demarre`
- [ ] PT: `Teefax [v...] iniciado`
- [ ] RU: `Teefax [v...] zapushchen`

---

## 14. Konfigurationsdatei (teefax.ini)

Testdatei `teefax.ini` anlegen:

```ini
--mute
--lang en
```

```
teefax 2s
```

- [ ] Kein Ton (`--mute` aus INI)
- [ ] Ausgabe auf Englisch (`--lang` aus INI)
- [ ] Kommandozeilenwert überschreibt INI:
  ```
  teefax 2s --lang de
  ```
  - [ ] Ausgabe auf Deutsch (CLI hat Vorrang)

INI danach wieder löschen / leeren.

---

## 15. Makro-System

### 15.1 Makro anlegen und ausführen

```
teefax --macro add tee 3m --mute --nomsg
```

- [ ] Bestätigung `Makro 'tee' gespeichert.` erscheint
- [ ] Eintrag in `teefax.ini` sichtbar (`macro tee = 3m --mute --nomsg`)

```
teefax --macro list
```

- [ ] Makro `tee` erscheint in der Liste

```
teefax tee
```

- [ ] Startet einen 3-Minuten-Timer (oder entsprechend den gespeicherten Argumenten)

### 15.2 Makro überschreiben

```
teefax --macro add tee 5m --mute --nomsg
```

- [ ] Rückfrage `... bereits vorhanden. Überschreiben?` erscheint
- [ ] Bei `n`: Makro bleibt unverändert
- [ ] Bei `j` / `y`: Makro wird aktualisiert

### 15.3 Makro entfernen

```
teefax --macro remove tee
```

- [ ] Bestätigung erscheint, Eintrag ist aus `teefax.ini` entfernt

```
teefax --macro remove nichtvorhanden
```

- [ ] Fehlermeldung, Exit-Code 1

### 15.4 Fehlerbehandlung

```
teefax --macro add bad-name 1s
```

- [ ] Fehlermeldung (ungültiger Name — Sonderzeichen nicht erlaubt), Exit-Code 1

INI danach wieder löschen / leeren.

---

## 16. Verhalten beim Doppelklick

`teefax.exe` direkt per Doppelklick starten (kein Terminal):

- [ ] Hilfe wird angezeigt
- [ ] `Weiter mit beliebiger Taste...` erscheint (pause-Prompt)
- [ ] Fenster schließt sich nach Tastendruck

---

## 17. Sonderfälle

```
teefax 5s --mute --nomsg
:: Während der Laufzeit mit der Maus in die Konsole klicken
```

- [ ] QuickEdit-Modus deaktiviert: Mausklick friert Timer **nicht** ein

```
teefax 5s --mute --nomsg
:: Strg+C während der Laufzeit
```

- [ ] Programm beendet sich sauber, kein hängender Prozess

```
teefax 5s --loop --mute --nomsg --cmd "teefax 2s --mute --nomsg"
:: Nach 2-3 Durchläufen Strg+C
```

- [ ] Beide Instanzen beenden sich, keine Zombie-Prozesse

---

## 18. Binärdatei

- [ ] Dateigröße plausibel (~1 MB ± 500 KB)
- [ ] Icon erscheint korrekt im Explorer (16×16, 32×32 erkennbar)
- [ ] Dateieigenschaften → Details: Firmenname, Produktname, Beschreibung, Copyright befüllt
- [ ] Version in den Dateieigenschaften stimmt mit `teefax --version` überein

---

## Ergebnis

| Bereich | Status |
|---|---|
| Smoke-Tests (automatisch) | ☐ OK ☐ Fehler |
| Anzeige & Ausgabe | ☐ OK ☐ Fehler |
| Ton | ☐ OK ☐ Fehler |
| Benachrichtigung | ☐ OK ☐ Fehler |
| Zielzeit --at | ☐ OK ☐ Fehler |
| Schleife --loop | ☐ OK ☐ Fehler |
| Voralarm --prealarm | ☐ OK ☐ Fehler |
| Täglicher Alarm --daily | ☐ OK ☐ Fehler |
| Wiederholender Alarm --every | ☐ OK ☐ Fehler |
| Aktionen nach Ablauf | ☐ OK ☐ Fehler |
| Interaktive Modi | ☐ OK ☐ Fehler |
| Sprachen | ☐ OK ☐ Fehler |
| INI-Datei | ☐ OK ☐ Fehler |
| Makro-System | ☐ OK ☐ Fehler |
| Doppelklick | ☐ OK ☐ Fehler |
| Sonderfälle | ☐ OK ☐ Fehler |
| Binärdatei | ☐ OK ☐ Fehler |

**Freigabe:** ☐ Release OK &nbsp;&nbsp; ☐ Nicht freigegeben | offene Punkte: ___________
