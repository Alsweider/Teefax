# Teefax
Teefax ist ein Küchenwecker, der über die Konsole bedient wird. Als Parameter erwartet er die gewünschte Wartezeit in Sekunden. Nach Ablauf der Zeit ertönt ein Signal. 
Ich habe lange [Timerle](https://www.jfsoftware.de/timerle.htm) verwendet, um Tee zu kochen. Der Nachteil daran ist, dass man die Zeit jedes Mal neu einstellen muss. Mit Teefax kann man einfach in der Kommandozeile den Befehl "teefax 180" eingeben, um sofort 3 Minuten abzuzählen.

Neueste Version: [Herunterladen](https://github.com/Alsweider/Teefax/releases/latest)

![2025-10-04 18_09_45-C__WINDOWS_system32_cmd exe - teefax  20](https://github.com/user-attachments/assets/7c9150eb-46fd-4215-84a9-9675a0c8333a)

# Nutzung

Es gibt mehrere Möglichkeiten, das Programm zu nutzen. Hier ein paar Beispiele:

1. Die komplizierteste Variante: In der Kommandozeile (cmd.exe) manuell in den Programmordner navigieren und das Programm mit einem Parameter ansprechen, z. B.: `cd C:\Tools\Teefax\` und dann `teefax 180`.
2. Eine Stapelverarbeitungs-Datei (.bat) anlegen, die das Programm mit der gewünschten Zeit in Sekunden anspricht: `@start "" "teefax.exe" 180`. Falls die Datei nicht im gleichen Ordner wie das Programm liegen soll, muss vor "teefax.exe" der vollständige Pfad angegeben werden: `@start "" "C:\Tools\teefax.exe" 180`
3. Anlegen einer Verknüpfung per Rechtsklick auf Teefax.exe. Damit das Programm immer die gewünschte Zeit abzählt, muss die Verknüpfung um die Sekundenzahl erweitert werden. Dazu klickt man mit der rechten Maustaste auf die Verknüpfung und öffnet die Eigenschaften. Dort ergänzt man im Ziel-Pfad einfach am Ende eine Zahl: `C:\Tools\Teefax\Teefax.exe 180`. Diese Verknüpfung lässt sich auch einfach in der Taskleiste plazieren.
4. Den Programmordner in den Systemeigenschaften als Systemumgebungsvariable definieren. Dazu ergänzt man den Pfad zum Ordner, in dem das Programm liegt, als neuen Eintrag in den Systemvariablen. Das führt dazu, dass das Programm als Befehl in der Kommandozeile direkt verfügbar wird, ohne dass man erst zum Ordner navigieren muss. Man kann also in der Ausführen-Zeile und in der Kommandozeile direkt `teefax 180` eingeben.

# Funktionsweise & Technik

Teefax misst zunächst den aktuellen Startzeitpunkt mit der hochpräzisen Klasse `chrono::steady_clock` aus der C++-Standardbibliothek, die speziell dafür gedacht ist, genaue physikalische Zeitintervalle zu messen. Sie läuft monoton, die Zeitpunkte können niemals rückwärts laufen und sie ist unabhängig von der Systemzeit des Betriebssystems sowie der Systemauslastung. Als nächstes wird in einer for-Schleife die Dauer bis zur jeweils nächsten Sekunde mit `chrono::seconds()` aus der Startzeit und der gewünschten Sekundenzahl errechnet. Mit `this_thread::sleep_until()` wird das Programm jeweils angehalten, bis dieser Zeitpunkt (die nächste Sekunde) erreicht ist.
Diese Vorgehensweise hat den Vorteil, dass sie präzise bleibt und immer bis zum errechneten Zielzeitpunkt wartet. Bei einem einfachen Sleep()-Befehl hingegen würde z. B. die Systemauslastung zu Abweichungen von mehreren Millisekunden pro Sekunde führen, die sich bei vielen Durchläufen aufsummieren und die gemessene Zeit deutlich verfälschen würden.
