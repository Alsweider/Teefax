# Teefax
Teefax ist ein Küchenwecker, der über die Konsole (Eingabeaufforderung) bedient wird. Als Parameter erwartet er die gewünschte Wartezeit oder Uhrzeit. Nach Ablauf der Zeit ertönt ein Signal.

Ich habe lange [Timerle](https://www.jfsoftware.de/timerle.htm) verwendet, um Tee zu kochen. Das Programm wurde seit 2006 nicht mehr aktualisiert, aber funktioniert unverändert. Ein Nachteil daran ist, dass man die Zeit jedes Mal neu einstellen muss. Auch wenn Timerle schon im Hintergrund aktiv ist, benötigt man mindestens zwei Klicks, um den Zähler zu starten. Mit Teefax dagegen kann man einfach den Befehl "teefax 180" verwenden, um sofort 3 Minuten abzuzählen. Über eine Verknüpfung sogar mit nur einem einzigen Klick.

Neueste Version: [Herunterladen](https://github.com/Alsweider/Teefax/releases/latest)

![teefax3](https://github.com/user-attachments/assets/b923ca54-9185-4cb9-a3c7-5686e9508086)
![teefax4](https://github.com/user-attachments/assets/f7bb81db-b6c0-45da-822e-8e5a7bac5682)

**Vorteile**
* Zähler mit einem Klick starten
* Beliebigen Weckton definieren (.WAV-Format)
* Wecken nach Dauer oder Uhrzeit
* Programmgröße < 100 KB
* Größe im Arbeitsspeicher: ~1,3 MB (~7,7 MB mit conhost.exe)
* Zugriff über Kommandozeile
* Wiederholfunktionen
* Datei nach einer bestimmten Zeit öffnen
* Konsolenbefehl zeitgesteuert ausführen
* genaue Zeitmessung ohne systemlastbedingte Schwankungen


# Nutzung

Es gibt mehrere Möglichkeiten, das Programm zu nutzen. Hier ein paar Beispiele:

1. Die komplizierteste Variante: In der Kommandozeile (cmd.exe) manuell in den Programmordner navigieren und das Programm mit einem Parameter ansprechen, z. B.: `cd C:\Tools\Teefax\` und dann `teefax 180`.
2. Eine Stapelverarbeitungs-Datei (.bat) anlegen, die das Programm mit der gewünschten Zeit in Sekunden anspricht: `@start "" "teefax.exe" 180`. Falls die Datei nicht im gleichen Ordner wie das Programm liegen sollte, muss vor "teefax.exe" der vollständige Pfad angegeben werden: `@start "" "C:\Tools\teefax.exe" 180`
3. Anlegen einer Verknüpfung per Rechtsklick auf Teefax.exe. Damit das Programm immer die gewünschte Zeit abzählt, muss die Verknüpfung um die Sekundenzahl oder weitere gewünschte Parameter erweitert werden. Dazu klickt man mit der rechten Maustaste auf die Verknüpfung und öffnet die Eigenschaften. Dort ergänzt man im Ziel-Pfad am Ende eine Zahl: `C:\Tools\Teefax\Teefax.exe 180`. Diese Verknüpfung lässt sich auch einfach in der Taskleiste plazieren.
4. Den Programmordner in den Systemeigenschaften als Systemumgebungsvariable definieren. Dazu ergänzt man den Pfad zum Ordner, in dem das Programm liegt, als neuen Path-Eintrag in den Systemvariablen. Das führt dazu, dass das Programm als Befehl in der Kommandozeile sowie im Ausführen-Dialog (Win+R) direkt verfügbar wird, ohne dass man erst zum Ordner navigieren muss.

Gestoppt wird das Programm durch Schließen des Konsolenfensters bzw. die Tastenkombination `Strg`+`C`.

# Parameter

Parameter sind die Daten (Variablen), die das Programm gerne verarbeiten möchte. Der Begriff `teefax` startet das Programm, aber ohne Daten kann es nichts tun. Deshalb ist mindestens die Sekundenzahl nötig. Teefax versteht aber noch weitere Einheiten und Anweisungen. Die Abkürzung für die Einheit steht immer direkt nach der entsprechenden Zahl. Zwischen Zahl und Einheit (`5m`) sowie zwischen kombinierten Einheiten (`5m30s`) werden keine Leerzeichen gesetzt.

- Millisekunden: `ms`
- Sekunden: `teefax 30` oder `teefax 30s` oder `teefax 30sec`
- Minuten: `m` (z. B.: `teefax 5m`) oder `min`
- Stunden: `h` oder `hr`
- Tage: `d` oder `day`
- Wochen: `w` oder `wk` oder  `week`
- Monate (auf 30 Tage genormt): `mo` oder `mon` oder `month`
- Jahre (auf 365 Tage genormt): `y` oder `yr` oder `year`
- Schleife: `--loop [Anzahl]` oder `-l` (z. B.: `teefax 3m --loop` oder `teefax 5s --loop 3` ). Der Zähler beginnt nach Ablauf der eingestellten Zeit wieder von vorne. Anzahl der Wiederholungen optional, sonst unendlich.
- Stummschalten: `--mute` (z. B. `teefax 20s --mute`) oder `-m`. Es wird kein Weckton abgespielt.
- Eigener Weckton: Um eine .WAV-Datei anstelle der Programmtöne abzuspielen, muss der Dateipfad in Anführungszeichen als Parameter angegeben werden, z. B.: `teefax 5m "C:\Musik\wassertropfen.WAV"`
- Uhrzeit (basierend auf Systemzeit): `--at HH:MM[:SS]` zählt bis zur angegebenen Uhrzeit.
- Alarm wiederholen: `--alarm-repeat [Anzahl]` oder `-ar [Anzahl]`
- Alarm-Intervall, Abstand zwischen den wiederholten Alarmen (Standard: 2): `--alarm-interval [Sekunden]` oder kurz `-ai [Sekunden]` (z. B. `teefax 10s --alarm-repeat 5 --alarm-interval 2` wiederholt den Alarmton nach Ablauf des Zählers fünfmal im 2-Sekunden-Takt)
- Weiterzählen während Alarm. Blockierung des Zählers durch Weckton umgehen, also parallel zum Ton zählen. (Sinnvoll etwa mit `--loop` und `--nomsg` einsetzbar.): `--async` oder `-as`
- Datei oder Programm nach Ablauf der Zeit öffnen: `-o [Dateipfad]` oder `--open [Dateipfad]`, z. B.: `teefax 5m -o "C:\Notizen\erledigen.txt"`
- Konsolenbefehl in der Kommandozeile ausführen: `-c [Befehl]` oder `--cmd [Befehl]`, z. B.: `teefax 20s --cmd "shutdown -s -f -t 180"` startet nach 20 Sekunden einen 180-Sekunden-Countdown zum Herunterfahren des Systems. Rekursiv: `teefax 5s -c "start teefax 20s"` startet nach Ablauf der 5 Sekunden einen neuen Zähler mit 20 Sekunden.
- Benachrichtigungsfenster (Popup) nach Ablauf des Zählers unterdrücken: `--nomsg`
- Bildschirmschoner & Ruhezustand unterdrücken: `-ns` oder `--nosleep`
- Akustisch runterzählen bis zum Alarm: `-pa [Sekunden]` oder `--prealarm [Sekunden]`. Beispiel: `teefax 20 --prealarm 5` wird 5 Sekunden vor dem Schlussalarm beginnen, sekündlich einen Signalton auszugeben. Mögliche Einsatzzwecke sind der Start eines Wettrennens oder auch Silvester. In Verbindung mit dem rekursiven Aufruf lassen sich auch Start- und Endzähler kombinieren. Dies etwa bewirkt einen 5-sekündigen Startzähler mit Signaltönen, der nach Ablauf einen 20-sekündigen Zähler mit 5-sekündigen Endsignalen startet: `teefax 5s --prealarm 5 --nomsg --cmd "start teefax 20s --prealarm 5"` 

Die Zeiteinheiten lassen sich auch kombiniert verwenden: `teefax 3m30s --loop --mute` Das löst einen Zähler von 3 Minuten und 30 Sekunden aus, der nach Ablauf keinen Weckton abspielt und sich immer wiederholt.

# Funktionsweise & Technik

Teefax misst zunächst den aktuellen Startzeitpunkt mit der hochpräzisen Klasse [chrono::steady_clock](https://cplusplus.com/reference/chrono/steady_clock/) aus der C++-Standardbibliothek ([2011](https://www.heise.de/blog/Zeit-in-C-20-Einfuehrung-in-die-Chrono-Terminologie-9642462.html) mit dem C++11-Standard eingeführt), die speziell dafür gedacht ist, genaue physikalische Zeitintervalle zu messen. Sie läuft monoton, die Zeitpunkte können niemals rückwärts laufen und sie ist unabhängig von der Systemzeit des Betriebssystems sowie der Systemauslastung. Als nächstes wird in einer for-Schleife die Dauer bis zur jeweils nächsten Sekunde mit [chrono::seconds()](https://cplusplus.com/reference/chrono/seconds/) aus der Startzeit und der gewünschten Sekundenzahl errechnet. Mit [this_thread::sleep_until()](https://cplusplus.com/reference/thread/this_thread/sleep_until/) wird das Programm dann angehalten, bis dieser Zeitpunkt (die nächste Sekunde) erreicht ist.
Diese Vorgehensweise hat den Vorteil, dass sie präzise bleibt und immer bis zum errechneten Zielzeitpunkt wartet. Bei einem einfachen Sleep()-Befehl hingegen würde etwa die Systemauslastung zu Abweichungen von mehreren Millisekunden pro Sekunde führen, die sich bei vielen Durchläufen aufsummieren und die gemessene Zeit deutlich verfälschen.


