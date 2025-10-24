#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <iomanip>
#include <ctime>
#include <windows.h>
#include <mmsystem.h>
#include <filesystem>
#include <limits>
#include <cctype>
#include <stdexcept>
#include <atomic>

#pragma comment(lib, "winmm.lib")

using namespace std;

namespace fs = std::filesystem;

// Globale Markierung, ob timeBeginPeriod(1) gesetzt wurde (für Ctrl-C Handler)
static atomic<bool> g_timePeriodSet{false};

// Maximale Millisekunden (wir nutzen die maximale long long - etwas konservativ geclamped)
constexpr long long MAX_MS = std::numeric_limits<long long>::max() / 4;

// Ctrl-C / Console Event Handler: versucht, timeEndPeriod zurückzusetzen
BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (g_timePeriodSet.load()) {
        timeEndPeriod(1);
        g_timePeriodSet.store(false);
    }
    // FALSE zurückgeben, damit der Standard-Handler ebenfalls ausgeführt wird (Programmterminierung)
    return FALSE;
}

// RAII-Hilfe: setzt die System-Timerauflösung auf 1 ms und sorgt für sauberes Zurücksetzen beim Verlassen
struct TimePeriodGuard {
    TimePeriodGuard() {
        MMRESULT res = timeBeginPeriod(1);
        g_timePeriodSet.store(true);
        (void)res;
    }
    ~TimePeriodGuard() {
        if (g_timePeriodSet.load()) {
            timeEndPeriod(1);
            g_timePeriodSet.store(false);
        }
    }
};

// Hilfsfunktionen: sichere Konvertierung
int safeStoi(const string& s, int fallback = 0) {
    try {
        size_t pos = 0;
        long v = stol(s, &pos, 10);
        if (pos != s.size()) return fallback;
        if (v < std::numeric_limits<int>::min()) return std::numeric_limits<int>::min();
        if (v > std::numeric_limits<int>::max()) return std::numeric_limits<int>::max();
        return static_cast<int>(v);
    } catch (...) {
        return fallback;
    }
}

double safeStod(const string& s, double fallback = 0.0) {
    try {
        size_t pos = 0;
        double v = stod(s, &pos);
        if (pos != s.size()) return fallback;
        return v;
    } catch (...) {
        return fallback;
    }
}

// Clamp-Funktion für Millisekunden (schützt vor Überlauf)
long long clampMs(long double ms) {
    if (ms <= 0.0L) return 0;
    if (ms > static_cast<long double>(MAX_MS)) return MAX_MS;
    long long res = static_cast<long long>(ms);
    if (res < 0) return MAX_MS;
    return res;
}

// Umrechnung in Millisekunden (mit Überlaufschutz, möglichst großer Bereich)
// Unterstützte Einheiten: ms, s/sec, m/min, h/hr/hour, d/day, w/wk/week,
// mo/mon/month (30d), y/yr/year (365d). Leere Einheit -> Sekunden.
long long unitToMilliseconds(double value, const string& unitPart) {
    string u;
    u.reserve(unitPart.size());
    for (char c : unitPart)
        u += static_cast<char>(tolower(static_cast<unsigned char>(c)));

    if (u == "ms") return clampMs(static_cast<long double>(value));
    else if (u == "s" || u == "sec" || u.empty()) return clampMs(static_cast<long double>(value) * 1000.0L);
    else if (u == "m" || u == "min") return clampMs(static_cast<long double>(value) * 60.0L * 1000.0L);
    else if (u == "h" || u == "hr" || u == "hour") return clampMs(static_cast<long double>(value) * 60.0L * 60.0L * 1000.0L);
    else if (u == "d" || u == "day") return clampMs(static_cast<long double>(value) * 24.0L * 60.0L * 60.0L * 1000.0L);
    else if (u == "w" || u == "wk" || u == "week") return clampMs(static_cast<long double>(value) * 7.0L * 24.0L * 60.0L * 60.0L * 1000.0L);
    else if (u == "mo" || u == "mon" || u == "month") return clampMs(static_cast<long double>(value) * 30.0L * 24.0L * 60.0L * 60.0L * 1000.0L);
    else if (u == "y" || u == "yr" || u == "year") return clampMs(static_cast<long double>(value) * 365.0L * 24.0L * 60.0L * 60.0L * 1000.0L);
    else {
        cout << "Unbekannte Einheit: " << unitPart << "\n";
        return 0;
    }
}

// Mehrteilige Zeitangabe analysieren (ohne regex, effizienter, robust)
// Unterstützt Kombinationen wie "1h20m30s" oder "1.5h" oder "90s"
long long parseTime(const string& arg) {
    long long totalMs = 0;
    size_t i = 0;
    const size_t n = arg.size();

    while (i < n) {
        while (i < n && isspace(static_cast<unsigned char>(arg[i]))) ++i;
        if (i >= n) break;

        size_t start = i;
        bool dot = false;
        while (i < n && (isdigit(static_cast<unsigned char>(arg[i])) || (!dot && arg[i] == '.'))) {
            if (arg[i] == '.') dot = true;
            ++i;
        }
        if (start == i) break;

        string numberStr = arg.substr(start, i - start);
        double value = safeStod(numberStr, -1.0);
        if (value < 0) return 0;

        size_t unitStart = i;
        while (i < n && isalpha(static_cast<unsigned char>(arg[i]))) ++i;
        string unit = arg.substr(unitStart, i - unitStart);

        long long addMs = unitToMilliseconds(value, unit);
        if (addMs > 0) {
            if (totalMs > MAX_MS - addMs) totalMs = MAX_MS;
            else totalMs += addMs;
        }
    }

    return totalMs;
}

// Hilfsfunktion: std::string -> std::wstring (mit Fehlerprüfung), weil Windows wstring für .WAV-Wiedergabe braucht
wstring toWide(const string& str) {
    if (str.empty()) return wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    if (size_needed == 0) return wstring();
    wstring wstr;
    wstr.resize(size_needed);
    int rc = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
    if (rc == 0) return wstring();
    if (!wstr.empty() && wstr.back() == L'\0') wstr.pop_back();
    return wstr;
}

// Berechnet Millisekunden bis zur nächsten angegebenen Uhrzeit
long long millisecondsUntilTime(int hour, int minute, int second = 0) {
    using namespace chrono;
    auto now = system_clock::now();
    time_t tnow = system_clock::to_time_t(now);
    tm local;
    if (localtime_s(&local, &tnow) != 0) return 0;

    local.tm_hour = hour;
    local.tm_min  = minute;
    local.tm_sec  = second;

    time_t target_t = mktime(&local);
    if (target_t == -1) return 0;
    auto target = system_clock::from_time_t(target_t);
    if (target <= now) target += hours(24);

    long long diff = duration_cast<milliseconds>(target - now).count();
    return clampMs(static_cast<long double>(diff));
}

void openFileAfterTimer(const string& filePath) {
    try {
        fs::path p(filePath);
        if (!fs::exists(p)) {
            cout << "\nDatei nicht gefunden: " << filePath << "\n";
            return;
        }
        wstring wfile = toWide(filePath);
        if (wfile.empty()) {
            cout << "\nFehler bei der Pfad-Konvertierung: " << filePath << "\n";
            return;
        }
        HINSTANCE res = ShellExecuteW(NULL, L"open", wfile.c_str(), NULL, NULL, SW_SHOWNORMAL);
        if ((INT_PTR)res <= 32) {
            cout << "\nFehler beim Öffnen der Datei (Code " << (INT_PTR)res << "): " << filePath << "\n";
        } else {
            cout << "\nDatei geöffnet: " << filePath << "\n";
        }
    } catch (const fs::filesystem_error& e) {
        cout << "\nDateisystem-Fehler beim Öffnen: " << e.what() << "\n";
    }
}

void runConsoleCommand(const string& command) {
    if (command.empty()) {
        cout << "\nKein Konsolenbefehl angegeben.\n";
        return;
    }

    // UTF-8 -> UTF-16 (notwendig für Windows CreateProcessW)
    wstring wcommand = toWide(command);
    if (wcommand.empty()) {
        cout << "\nFehler bei der Befehls-Konvertierung: " << command << "\n";
        return;
    }

    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    // /C bewirkt, dass die Eingabe nach Ausführung geschlossen wird. Bei /K bleibt sie offen.
    wstring fullCmd = L"cmd.exe /C " + wcommand;

    BOOL success = CreateProcessW(
        NULL,
        &fullCmd[0],
        NULL, NULL,
        TRUE,//vorher "FALSE,", wir nehmen mal TRUE, um die Ausgabe von --cmd "dir" zu sehen (bInheritHandles / Handle-Vererbung)
        0, //CREATE_NO_WINDOW, // kein Konsolenfenster anzeigen // 0, eben doch!
        NULL, NULL,
        &si, &pi
        );

    if (!success) {
        DWORD err = GetLastError();
        cout << "\nFehler beim Ausfuehren des Befehls (" << err << "): " << command << "\n";
        return;
    }

    // Warten, bis der Befehl beendet ist (optional)
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    cout << "\nBefehl ausgefuehrt: " << command << "\n";
}

// Benachrichtigung / MessageBox
void showNotification(const std::wstring& title, const std::wstring& message)
{
    // Attrappenfenster, hehehehe! Um den Fokus zu schnappen.
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"STATIC",
        L"",
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr
        );

    if (hwnd)
    {
        // Fenster wirklich nach oben bringen
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

        // Fokusanforderung
        SetForegroundWindow(hwnd);
        BringWindowToTop(hwnd);
        SetActiveWindow(hwnd);

        // MessageBox im Vordergrund anzeigen, mit MB_SETFOREGROUND als Rückversicherung
        MessageBoxW(hwnd, message.c_str(), title.c_str(),
                    MB_OK | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);

        DestroyWindow(hwnd);
    }
    else
    {
        // Rückfall, wenn das Fenster nicht erzeugt wurde
        MessageBoxW(nullptr, message.c_str(), title.c_str(),
                    MB_OK | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);
    }
}

// alt:
// void showNotification(const std::wstring& title, const std::wstring& message) {
//     MessageBoxW(nullptr, message.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
// }

// Hauptprogramm
int main(int argc, char* argv[])
{
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    TimePeriodGuard timeGuard;

    if (argc < 2) {
        cout << "Verwendung:\n"
             << "  teefax [<Zeit>] [Sounddatei] [Optionen]\n\n"
             << "Optionen:\n"
             << "  -m,  --mute                 Kein Weckton abspielen\n"
             << "  -l,  --loop [Anzahl]        Wiederhole den Timer (Anzahl optional, sonst unendlich)\n"
             << "  -ar, --alarm-repeat <n>     Anzahl der Weckton-Wiederholungen (Standard: 1)\n"
             << "  -ai, --alarm-interval <s>   Abstand zwischen Wiederholungen in Sekunden (Standard: 2)\n"
             << "       --at HH:MM[:SS]        Starte bis zur angegebenen Uhrzeit\n"
             << "       --async, -as           Spielt den Ton asynchron (Blockierung umgehen)\n"
             << "  -o,  --open <Dateipfad>     Oeffnet nach Ablauf des Zaehlers die angegebene Datei\n\n"
             << "  -c,  --cmd  <Befehl>        Fuehrt nach Ablauf einen Konsolenbefehl aus\n"
             << "Beispiele:\n"
             << "  teefax 5m\n"
             << "  teefax 10s --loop\n"
             << "  teefax --loop 5 3s\n"
             << "  teefax --at 07:30\n"
             << "  teefax --at 07:30:15 \"C:\\Klang\\gong.wav\"\n"
             << "  teefax 3s --async \"C:\\Klang\\gong.wav\"\n"
             << "  teefax 10s --cmd \"shutdown /s /t 0\"\n"
             << "  teefax 5m -c \"start notepad.exe\"\n";
        return 1;
    }

    string soundFile;
    bool mute = false;
    bool loop = false;
    int maxLoops = -1;
    int alarmRepeat = 1;
    int alarmInterval = 2;
    bool useAtTime = false;
    int atHour = 0, atMinute = 0, atSecond = 0;
    long long ms = 0;
    bool asyncSound = false;
    string openFile;
    bool showMessage = true; // Standard: MessageBox ist aktiv

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];

        if (arg == "--mute" || arg == "-m") mute = true;
        else if (arg == "--loop" || arg == "-l") {
            loop = true;
            if (i + 1 < argc) {
                int possibleCount = safeStoi(argv[i + 1], -1);
                if (possibleCount > 0) {
                    maxLoops = possibleCount;
                    ++i;
                }
            }
        }
        else if (arg == "--nomsg") {
            showMessage = false;
        }
        else if ((arg == "--alarm-repeat" || arg == "-ar") && i + 1 < argc) {
            alarmRepeat = safeStoi(argv[++i], 1);
            if (alarmRepeat < 1) alarmRepeat = 1;
        }
        else if ((arg == "--alarm-interval" || arg == "-ai") && i + 1 < argc) {
            alarmInterval = safeStoi(argv[++i], 2);
            if (alarmInterval < 1) alarmInterval = 1;
        }
        else if (arg == "--async" || arg == "-as") asyncSound = true;
        else if (arg == "--at" && i + 1 < argc) {
            string timeStr = argv[++i];
            int parsed = sscanf(timeStr.c_str(), "%d:%d:%d", &atHour, &atMinute, &atSecond);
            if (parsed < 2) {
                cout << "Ungueltiges Zeitformat fuer --at. Erwartet HH:MM[:SS]\n";
                return 1;
            }
            if (parsed == 2) atSecond = 0;
            useAtTime = true;
            ms = millisecondsUntilTime(atHour, atMinute, atSecond);
            if (ms == 0) {
                cout << "Fehler bei der Berechnung der Zielzeit fuer --at.\n";
                return 1;
            }
        }
        else if ((arg == "--open" || arg == "-o") && i + 1 < argc) {
            openFile = argv[++i];
        }
        else if ((arg == "--cmd" || arg == "-c") && i + 1 < argc) {
            openFile = ""; // sicherstellen, dass nur eine Aktion aktiv ist
            string cmdArg = argv[++i];
            openFile = "[CMD]" + cmdArg; // Markierung, damit später erkannt wird
        }
        else if (!useAtTime) {
            long long possible = parseTime(arg);
            if (possible > 0) ms = possible;
            else if (soundFile.empty()) soundFile = arg;
        }
        else if (soundFile.empty()) soundFile = arg;
    }

    if (!useAtTime && ms <= 0) {
        cout << "Bitte eine gueltige Zeit oder --at angeben.\n";
        return 1;
    }

    if (ms > MAX_MS) ms = MAX_MS;

    cout << "Teefax v" << PRG_VERSION << " gestartet";
    if (useAtTime)
        cout << " fuer Uhrzeit " << setfill('0') << setw(2) << atHour << ":"
             << setw(2) << atMinute << ":" << setw(2) << atSecond;
    else
        cout << " mit Zaehler: " << (ms / 1000) << " Sekunden";
    if (asyncSound) cout << " (async sound)";
    cout << "\n";

    const int barWidth = 30;
    int loopCount = 0;

    do {
        if (loop) ++loopCount;
        auto start = chrono::steady_clock::now();
        auto end = start + chrono::milliseconds(ms);

        long long lastVerbleibendSec = -1;
        const chrono::milliseconds tickInterval(500);
        auto nextTick = chrono::steady_clock::now() + tickInterval;

        while (chrono::steady_clock::now() < end) {
            auto nowTime = chrono::steady_clock::now();
            auto verbleibendMs = chrono::duration_cast<chrono::milliseconds>(end - nowTime).count();
            if (verbleibendMs < 0) verbleibendMs = 0;
            long long verbleibendSec = (verbleibendMs + 999) / 1000;

            if (verbleibendSec != lastVerbleibendSec) {
                lastVerbleibendSec = verbleibendSec;
                long long totalMs = ms;
                long long elapsedMs = totalMs - verbleibendMs;
                if (elapsedMs < 0) elapsedMs = 0;
                long double fraction = (totalMs > 0) ? (static_cast<long double>(elapsedMs) / static_cast<long double>(totalMs)) : 1.0L;
                if (fraction < 0.0L) fraction = 0.0L;
                if (fraction > 1.0L) fraction = 1.0L;
                int filled = static_cast<int>(fraction * barWidth);
                if (filled > barWidth) filled = barWidth;

                int minutes = static_cast<int>(verbleibendSec / 60);
                int seconds = static_cast<int>(verbleibendSec % 60);

                cout << "\r";
                if (loop) cout << "Durchlauf " << loopCount << " | ";
                cout << "Verbleibend: " << setfill('0') << setw(2) << minutes << ":"
                     << setw(2) << seconds << " [";
                for (int i = 0; i < barWidth; ++i) cout << (i < filled ? '#' : '-');
                cout << "]   " << flush;
            }

            nextTick += tickInterval;
            auto nowForSleep = chrono::steady_clock::now();
            if (nextTick <= nowForSleep) nextTick = nowForSleep + tickInterval;
            this_thread::sleep_until(nextTick);
        }

        cout << "\r";
        if (loop) cout << "Durchlauf " << loopCount << " | ";
        cout << "Verbleibend: 00:00 [";
        for (int i = 0; i < barWidth; ++i) cout << '#';
        cout << "]   " << flush;

        if (!mute) {
            for (int r = 0; r < alarmRepeat; ++r) {
                if (!soundFile.empty()) {
                    try {
                        fs::path p(soundFile);
                        if (!fs::exists(p) || !fs::is_regular_file(p)) {
                            cout << "\nAudiodatei nicht gefunden oder keine regulaere Datei: " << soundFile << "\n";
                        } else {
                            wstring widePath = toWide(soundFile);
                            if (!widePath.empty()) {
                                UINT flags = SND_FILENAME | (asyncSound ? SND_ASYNC : SND_SYNC);
                                PlaySoundW(widePath.c_str(), NULL, flags);
                            } else {
                                cout << "\nFehler bei Pfad-Konvertierung fuer Audiodatei: " << soundFile << "\n";
                            }
                        }
                    } catch (const fs::filesystem_error& e) {
                        cout << "\nDateisystem-Fehler: " << e.what() << "\n";
                    }
                } else {
                    if (asyncSound) {
                        thread([](){
                            Beep(880, 300);
                            Beep(988, 300);
                            Beep(1047, 500);
                        }).detach();
                    } else {
                        Beep(880, 300);
                        Beep(988, 300);
                        Beep(1047, 500);
                    }
                }
                if (r < alarmRepeat - 1) this_thread::sleep_for(chrono::seconds(alarmInterval));
            }
        }

        //Datei öffnen oder Konsolenbefehl ausführen
        if (!openFile.empty()) {
            if (openFile.rfind("[CMD]", 0) == 0) {
                string command = openFile.substr(5);
                runConsoleCommand(command);
            } else {
                openFileAfterTimer(openFile);
            }
        }

        //Benachrichtigung, Zeit abgelaufen
        if (showMessage) {
            showNotification(L"Teefax", L"Die Zeit ist verstrichen!");
        }

        if (useAtTime) {
            long long nextMs = millisecondsUntilTime(atHour, atMinute, atSecond);
            if (nextMs == 0) {
                cout << "\nFehler bei der Berechnung der naechsten Uhrzeit.\n";
                return 1;
            }
            ms = nextMs;
            if (ms > MAX_MS) ms = MAX_MS;
        }

    } while (loop && (maxLoops == -1 || loopCount < maxLoops));

    cout << "\nZaehler beendet.\n";
    return 0;
}
