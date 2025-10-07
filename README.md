# Teefax
Teefax ist ein Küchenwecker, der über die Konsole (Eingabeaufforderung) bedient wird. Als Parameter erwartet er die gewünschte Wartezeit in Sekunden. Nach Ablauf der Zeit ertönt ein Signal.

Ich habe lange [Timerle](https://www.jfsoftware.de/timerle.htm) verwendet, um Tee zu kochen. Das Programm wurde seit 2006 nicht mehr aktualisiert, aber funktioniert unverändert. Ein Nachteil daran ist, dass man die Zeit jedes Mal neu einstellen muss. Auch wenn Timerle schon im Hintergrund aktiv ist, benötigt man mindestens zwei Klicks, um den Zähler zu starten. Mit Teefax dagegen kann man einfach den Befehl "teefax 180" verwenden, um sofort 3 Minuten abzuzählen. Über eine Verknüpfung sogar mit nur einem einzigen Klick.

Neueste Version: [Herunterladen](https://github.com/Alsweider/Teefax/releases/latest)

![2025-10-04 18_09_45-C__WINDOWS_system32_cmd exe - teefax  20](https://github.com/user-attachments/assets/7c9150eb-46fd-4215-84a9-9675a0c8333a)


**Vorteile**
* Zähler mit einem Klick starten
* Programmgröße < 150 KB
* Größe im Arbeitsspeicher: ~0,5 MB (~7 MB mit conhost.exe)
* Zugriff über Kommandozeile
* genaue Zeitmessung ohne systemlastbedingte Schwankungen

# Nutzung

Es gibt mehrere Möglichkeiten, das Programm zu nutzen. Hier ein paar Beispiele:

1. Die komplizierteste Variante: In der Kommandozeile (cmd.exe) manuell in den Programmordner navigieren und das Programm mit einem Parameter ansprechen, z. B.: `cd C:\Tools\Teefax\` und dann `teefax 180`.
2. Eine Stapelverarbeitungs-Datei (.bat) anlegen, die das Programm mit der gewünschten Zeit in Sekunden anspricht: `@start "" "teefax.exe" 180`. Falls die Datei nicht im gleichen Ordner wie das Programm liegen sollte, muss vor "teefax.exe" der vollständige Pfad angegeben werden: `@start "" "C:\Tools\teefax.exe" 180`
3. Anlegen einer Verknüpfung per Rechtsklick auf Teefax.exe. Damit das Programm immer die gewünschte Zeit abzählt, muss die Verknüpfung um die Sekundenzahl erweitert werden. Dazu klickt man mit der rechten Maustaste auf die Verknüpfung und öffnet die Eigenschaften. Dort ergänzt man im Ziel-Pfad am Ende eine Zahl: `C:\Tools\Teefax\Teefax.exe 180`. Diese Verknüpfung lässt sich auch einfach in der Taskleiste plazieren.
4. Den Programmordner in den Systemeigenschaften als Systemumgebungsvariable definieren. Dazu ergänzt man den Pfad zum Ordner, in dem das Programm liegt, als neuen Path-Eintrag in den Systemvariablen. Das führt dazu, dass das Programm als Befehl in der Kommandozeile direkt verfügbar wird, ohne dass man erst zum Ordner navigieren muss. Man kann also im Ausführen-Dialog und in der Kommandozeile direkt `teefax 180` eingeben.

# Parameter

Parameter sind die Daten (Variablen), die das Programm gerne verarbeiten möchte. Der Begriff `teefax` startet das Programm, aber ohne Daten kann es nichts tun. Deshalb ist mindestens die Sekundenzahl nötig. Teefax versteht aber noch weitere Einheiten und Anweisungen. Die Abkürzung für die Einheit steht immer nach der entsprechenden Zahl:

- Sekunden: `teefax 30` oder teefax `30s`
- Minuten: `m` (z. B.: `teefax 5m`
- Stunden: `h`
- Tage: `d`
- Wochen: `w`
- Monate (30 Tage genormt): `mo`
- Jahre: `y`
- Schleife: `--loop` (z. B.: `teefax 3m --loop`). Der Zähler beginnt nach Ablauf der eingestellten Zeit wieder von vorne.
- Stummschalten: `--mute` (z. B. `teefax 20s --mute`). Es wird kein Weckton abgespielt.
- Eigener Weckton: Um eine .WAV-Datei anstelle der Programmtöne abzuspielen, muss der Dateipfad in Anführungszeichen als Parameter angegeben werden, z. B.: `teefax 5m "C:\Musik\wassertropfen.WAV"`

Die Zeiteinheiten lassen sich auch kombiniert verwenden: `teefax 3m30s --loop --mute` 

_ 

# Funktionsweise & Technik

Teefax misst zunächst den aktuellen Startzeitpunkt mit der hochpräzisen Klasse [chrono::steady_clock](https://cplusplus.com/reference/chrono/steady_clock/) aus der C++-Standardbibliothek ([2011](https://www.heise.de/blog/Zeit-in-C-20-Einfuehrung-in-die-Chrono-Terminologie-9642462.html) mit dem C++11-Standard eingeführt), die speziell dafür gedacht ist, genaue physikalische Zeitintervalle zu messen. Sie läuft monoton, die Zeitpunkte können niemals rückwärts laufen und sie ist unabhängig von der Systemzeit des Betriebssystems sowie der Systemauslastung. Als nächstes wird in einer for-Schleife die Dauer bis zur jeweils nächsten Sekunde mit [chrono::seconds()](https://cplusplus.com/reference/chrono/seconds/) aus der Startzeit und der gewünschten Sekundenzahl errechnet. Mit [this_thread::sleep_until()](https://cplusplus.com/reference/thread/this_thread/sleep_until/) wird das Programm dann angehalten, bis dieser Zeitpunkt (die nächste Sekunde) erreicht ist.
Diese Vorgehensweise hat den Vorteil, dass sie präzise bleibt und immer bis zum errechneten Zielzeitpunkt wartet. Bei einem einfachen Sleep()-Befehl hingegen würde etwa die Systemauslastung zu Abweichungen von mehreren Millisekunden pro Sekunde führen, die sich bei vielen Durchläufen aufsummieren und die gemessene Zeit deutlich verfälschen.


