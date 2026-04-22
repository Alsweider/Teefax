#include <iostream>
#include "i18n.h" // Übersetzungen
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
#include <atomic>
#include "sound_array.h" // Signalton
#include <vector> // Für täglichen Alarm


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
        char buf[256];
        snprintf(buf, sizeof(buf), t(Str::ERROR_UNKNOWN_UNIT), unitPart.c_str());
        cout << buf;
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

// Prüft, ob ein Argument eine bekannte Zeiteinheit ohne vorangestellte Zahl ist
// (Erkennt Tippfehler wie "teefax 5 min" statt "teefax 5min")
bool isStandaloneUnit(const string& arg) {
    string u;
    u.reserve(arg.size());
    for (char c : arg)
        u += static_cast<char>(tolower(static_cast<unsigned char>(c)));
    static const vector<string> known = {
        "ms", "s", "sec", "m", "min", "h", "hr", "hour",
        "d", "day", "w", "wk", "week", "mo", "mon", "month",
        "y", "yr", "year"
    };
    for (const auto& k : known)
        if (u == k) return true;
    return false;
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
    local.tm_isdst = -1; // Zeitumstellung

    time_t target_t = mktime(&local);
    if (target_t == -1) return 0;
    auto target = system_clock::from_time_t(target_t);
    if (target <= now) target += hours(24);

    long long diff = duration_cast<milliseconds>(target - now).count();
    return clampMs(static_cast<long double>(diff));
}


// Berechnet Millisekunden bis zu einem bestimmten Datum und Uhrzeit
long long millisecondsUntilDateTime(int year, int month, int day,
                                    int hour, int minute, int second)
{
    using namespace chrono;

    auto now = system_clock::now();

    tm target_tm{};
    target_tm.tm_year = year - 1900;
    target_tm.tm_mon  = month - 1;
    target_tm.tm_mday = day;
    target_tm.tm_hour = hour;
    target_tm.tm_min  = minute;
    target_tm.tm_sec  = second;
    target_tm.tm_isdst = -1; // Sommerzeit automatisch bestimmen

    time_t target_t = mktime(&target_tm);
    if (target_t == -1)
        return 0;

    auto target = system_clock::from_time_t(target_t);

    if (target <= now)
        return 0; // Zeitpunkt liegt in der Vergangenheit

    long long diff =
        duration_cast<milliseconds>(target - now).count();

    return clampMs(static_cast<long double>(diff));
}


void openFileAfterTimer(const string& filePath) {
    bool isUrl = filePath.rfind("http://", 0) == 0 ||
                 filePath.rfind("https://", 0) == 0;

    if (!isUrl) {
        try {
            if (!fs::exists(fs::path(filePath))) {
                cout << t(Str::FILE_NOT_FOUND) << filePath << "\n";
                return;
            }
        } catch (const fs::filesystem_error& e) {
            char buf[512];
            snprintf(buf, sizeof(buf), t(Str::FILE_SYSTEM_ERROR), e.what());
            cout << buf << "\n";
            return;
        }
    }

    wstring wfile = toWide(filePath);
    if (wfile.empty()) {
        cout << t(Str::ERROR_FILE_CONVERSION) << filePath << "\n";
        return;
    }
    HINSTANCE res = ShellExecuteW(NULL, L"open", wfile.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)res <= 32) {
        char buf[256];
        snprintf(buf, sizeof(buf), t(Str::FILE_ERROR), (INT_PTR)res, filePath.c_str());
        cout << buf;
    } else {
        char buf[256];
        snprintf(buf, sizeof(buf), t(Str::FILE_OPENED), filePath.c_str());
        cout << buf;
    }
}

void runConsoleCommand(const string& command) {
    if (command.empty()) {
        cout << t(Str::NO_COMMAND) << "\n";
        return;
    }

    wstring wcommand = toWide(command);
    if (wcommand.empty()) {
        char buf[512];
        snprintf(buf, sizeof(buf), t(Str::ERROR_CMD_CONVERSION), command.c_str());
        cout << buf << "\n";
        return;
    }

    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    wstring fullCmd = L"cmd.exe /C " + wcommand;

    BOOL success = CreateProcessW(
        NULL, &fullCmd[0],
        NULL, NULL, TRUE, 0,
        NULL, NULL, &si, &pi
        );

    if (!success) {
        DWORD err = GetLastError();
        char buf[512];
        snprintf(buf, sizeof(buf), t(Str::CMD_ERROR), (int)err, command.c_str());
        cout << buf << "\n";
        return;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    char buf[512];
    snprintf(buf, sizeof(buf), t(Str::CMD_STARTED), command.c_str());
    cout << buf << "\n";
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

void preventSleep(bool enable)
{
#ifdef _WIN32
    if (enable)
        SetThreadExecutionState(
            ES_CONTINUOUS |
            ES_SYSTEM_REQUIRED |
            ES_DISPLAY_REQUIRED
            );
    else
        SetThreadExecutionState(ES_CONTINUOUS);
#else
    // Platzhalter für Linux/macOS
#endif
}


// Hilfsfunktion: Sekunden in Jahre/Monate/Tage/Stunden/Minuten/Sekunden aufteilen
string formatVerbleibend(long long totalSec) {
    constexpr int secPerMin  = 60;
    constexpr int secPerHour = 60 * secPerMin;
    constexpr int secPerDay  = 24 * secPerHour;
    constexpr int secPerMonth = 30 * secPerDay;
    constexpr int secPerYear = 365 * secPerDay;

    long long years   = totalSec / secPerYear; totalSec %= secPerYear;
    long long months  = totalSec / secPerMonth; totalSec %= secPerMonth;
    long long days    = totalSec / secPerDay; totalSec %= secPerDay;
    long long hours   = totalSec / secPerHour; totalSec %= secPerHour;
    long long minutes = totalSec / secPerMin; totalSec %= secPerMin;
    long long seconds = totalSec;

    struct UnitVal { long long val; const char* unit; };
    UnitVal parts[] = {
        {years, "y"}, {months, "mo"}, {days, "d"},
        {hours, "h"}, {minutes, "m"}, {seconds, "s"}
    };

    // Letzten Nicht-Null-Wert finden
    int last = -1;
    for (int i = 5; i >= 0; --i) {
        if (parts[i].val > 0) { last = i; break; }
    }
    if (last == -1) return "0s";

    // Ausgabe vom ersten Nicht-Null bis zum letzten Nicht-Null
    stringstream ss;
    bool found = false;
    for (int i = 0; i <= last; ++i) {
        if (parts[i].val > 0 || found) {
            if (found) ss << " ";
            ss << parts[i].val << parts[i].unit;
            found = true;
        }
    }

    return ss.str();
}

// Ja, das sagt dir halt, ob die Zielzeit (--at) auf den morgigen Tag fällt.
bool isTargetTomorrow(time_t targetT) {
    time_t tnow = time(nullptr);

    tm nowTm{};
    localtime_s(&nowTm, &tnow);

    // heutiger Tag +1
    nowTm.tm_mday += 1;
    mktime(&nowTm); // normalisiert (Monats-/Jahreswechsel)

    tm targetTm{};
    localtime_s(&targetTm, &targetT);

    return (targetTm.tm_year == nowTm.tm_year &&
            targetTm.tm_mon  == nowTm.tm_mon  &&
            targetTm.tm_mday == nowTm.tm_mday);
}

// Zum Messen bis zum nächsten täglichen Alarm
long long millisecondsUntilNextDailyTime(const vector<tuple<int,int,int>>& times) {
    using namespace chrono;

    auto now = system_clock::now();
    time_t tnow = system_clock::to_time_t(now);

    tm local{};
    localtime_s(&local, &tnow);

    long long bestMs = MAX_MS;

    for (const auto& t : times) {
        int h, m, s;
        tie(h, m, s) = t;

        tm candidate = local;
        candidate.tm_hour = h;
        candidate.tm_min  = m;
        candidate.tm_sec  = s;
        candidate.tm_isdst = -1; // Zeitumstellung

        time_t tt = mktime(&candidate);
        if (tt == -1) continue;

        auto target = system_clock::from_time_t(tt);

        if (target <= now) {
            candidate.tm_mday += 1;
            candidate.tm_isdst = -1;
            time_t tt_next = mktime(&candidate);
            if (tt_next == -1) continue;
            target = system_clock::from_time_t(tt_next);
        }

        long long diff = duration_cast<milliseconds>(target - now).count();

        if (diff > 0 && diff < bestMs)
            bestMs = diff;
    }

    return bestMs == MAX_MS ? 0 : bestMs;
}


chrono::system_clock::time_point nextDailyTarget(
    const vector<tuple<int,int,int>>& times)
{
    using namespace chrono;
    auto now = system_clock::now();
    time_t tnow = system_clock::to_time_t(now);
    tm local{};
    localtime_s(&local, &tnow);

    time_t best = -1;

    for (const auto& t : times) {
        int h, m, s;
        tie(h, m, s) = t;

        tm candidate = local;
        candidate.tm_hour  = h;
        candidate.tm_min   = m;
        candidate.tm_sec   = s;
        candidate.tm_isdst = -1;

        time_t tt = mktime(&candidate);
        if (tt == -1) continue;

        auto target = system_clock::from_time_t(tt);
        // if (target <= now) target += hours(24);
        if (target <= now) {
            candidate.tm_mday += 1;
            candidate.tm_isdst = -1;
            time_t tt_next = mktime(&candidate);
            if (tt_next == -1) continue;
            target = system_clock::from_time_t(tt_next);
        }

        time_t tt2 = system_clock::to_time_t(target);
        if (best == -1 || tt2 < best)
            best = tt2;
    }

    return best != -1
               ? system_clock::from_time_t(best)
               : system_clock::now() + seconds(1);
}

// Erkennt, ob Teefax aus einer bestehenden Konsole aufgerufen wurde
// oder ob Windows selbst die Konsole erstellt hat (= Doppelklick)
bool launchedFromExistingConsole() {
    HWND hwnd = GetConsoleWindow();
    if (!hwnd) return false;

    DWORD consoleProc = 0;
    GetWindowThreadProcessId(hwnd, &consoleProc);

    // Wenn die Konsole einem anderen Prozess gehört, wurde sie geerbt
    return (consoleProc != GetCurrentProcessId());
}

// Hauptprogramm
int main(int argc, char* argv[])
{
    //Konsolenausgaben flüssiger machen
    ios::sync_with_stdio(false); // Trennt C++-Streams von C-Streams, beschleunigt cin/cout
    cout.tie(nullptr); // Löst Bindung von cout an cin, verhindert automatisches Flush vor Eingabe

    //Variablen definieren
    string soundFile;
    bool mute = false;
    bool loop = false;
    int maxLoops = -1;
    int alarmRepeat = 1;
    int alarmInterval = 2;
    bool useAtTime = false;
    bool useAtDateTime = false; // true wenn --at ein Datum (+ Zeit) enthält
    int atYear = 0, atMonth = 0, atDay = 0;
    int atHour = 0, atMinute = 0, atSecond = 0;
    long long ms = 0;
    bool asyncSound = false;
    string openFile;
    string cmdArg;
    bool showMessage = true; // Standard: MessageBox ist aktiv
    bool noSleep = false; // Bildschirmschoner & Standby nicht unterdrücken
    int preAlarmSeconds = 0; // Sekunden vor Schluss, in denen sekündlich gepiept wird
    const int barWidth = 30;
    int loopCount = 0;
    bool showLiveTime = false; // Für die direkte Zeitanzeige, wie der Name schon sagt.
    vector<tuple<int,int,int>> dailyTimes;
    bool useDailyTimes = false;
    string customMsg; // benutzerdefinierte Notiz in Benachrichtigungsfenster am Schluss

    // Audio priorisieren
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    TimePeriodGuard timeGuard;

    // Sprache vorab setzen, damit currentLang() korrekt initialisiert wird
    for (int i = 1; i < argc; ++i) {
        string a = argv[i];
        if ((a == "--lang" || a == "-la") && i + 1 < argc) {
            _putenv_s("TEEFAX_LANG", argv[i + 1]);
            break;
        }
    }

    if (argc < 2) {
        char buf[256];
        snprintf(buf, sizeof(buf), t(Str::STARTED), PRG_VERSION);
        cout << buf << ".\n\n";
        cout << t(Str::USAGE_HEADER);

        if (!launchedFromExistingConsole()) {
            cout << "\n";
            system("pause"); // oder: cout << "Druecke eine Taste..."; cin.get();
        }

        return 0;
    }

    // Argument-Parsen
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];

        if (arg == "--nosleep" || arg == "-ns") {
            noSleep = true;
        } else if (arg == "--mute" || arg == "-m") {
            mute = true;
        } else if (arg == "--loop" || arg == "-l") {
            loop = true;
            if (i + 1 < argc) {
                int possibleCount = safeStoi(argv[i + 1], -1);
                if (possibleCount > 0) {
                    maxLoops = possibleCount;
                    ++i;
                }
            }
        } else if (arg == "--nomsg") {
            showMessage = false;
        } else if (arg == "--msg" && i + 1 < argc) {
        customMsg = argv[++i];
        } else if ((arg == "--alarm-repeat" || arg == "-ar") && i + 1 < argc) {
            alarmRepeat = safeStoi(argv[++i], 1);
            if (alarmRepeat < 1) alarmRepeat = 1;
        } else if ((arg == "--alarm-interval" || arg == "-ai") && i + 1 < argc) {
            alarmInterval = safeStoi(argv[++i], 2);
            if (alarmInterval < 1) alarmInterval = 1;
        } else if (arg == "--async" || arg == "-as") {
            asyncSound = true;
        } else if ((arg == "--at" || arg == "-a" || arg == "--until") && i + 1 < argc) {
            string first = argv[++i];

            int year, month, day;
            int hour = 0, minute = 0, second = 0;

            // Fall 1: Datum erkannt
            if (sscanf(first.c_str(), "%d-%d-%d", &year, &month, &day) == 3)
            {
                // Prüfen, ob danach eine Uhrzeit folgt (kein weiterer Parameter oder beginnt mit '-')
                if (i + 1 < argc && argv[i + 1][0] != '-')
                {
                    string timeStr = argv[i + 1];

                    int parsed = sscanf(timeStr.c_str(),
                                        "%d:%d:%d",
                                        &hour, &minute, &second);

                    if (parsed >= 2)
                    {
                        if (parsed == 2) second = 0;
                        ++i;
                    }
                    else
                    {
                        // Keine gültige Uhrzeit → Mitternacht verwenden
                        hour = 0;
                        minute = 0;
                        second = 0;
                    }
                }

                atYear   = year;
                atMonth  = month;
                atDay    = day;
                atHour   = hour;
                atMinute = minute;
                atSecond = second;

                useAtTime     = true;
                useAtDateTime = true;

                ms = millisecondsUntilDateTime(
                    year, month, day,
                    hour, minute, second);

                if (ms == 0)
                {
                    cout << t(Str::ERROR_PAST_DATETIME) << "\n";
                    return 1;
                }
            }
            else
            {
                // Fall 2: Nur Uhrzeit (wie bisher)
                int parsed = sscanf(first.c_str(),
                                    "%d:%d:%d",
                                    &atHour, &atMinute, &atSecond);

                if (parsed < 2)
                {
                    cout << t(Str::ERROR_INVALID_AT) << "\n";
                    return 1;
                }

                if (parsed == 2) atSecond = 0;

                useAtTime = true;

                ms = millisecondsUntilTime(
                    atHour, atMinute, atSecond);

                if (ms == 0)
                {
                    cout << t(Str::ERROR_NEXT_TIME);
                    return 1;
                }
            }
        } else if ((arg == "--open" || arg == "-o") && i + 1 < argc) {
            openFile = argv[++i];
        } else if ((arg == "--cmd" || arg == "-c") && i + 1 < argc) {
            cmdArg = argv[++i];
        } else if ((arg == "--prealarm" || arg == "-pa") && i + 1 < argc) { // Sekündliches Piepsen vor Schluss
            preAlarmSeconds = safeStoi(argv[++i], 0);
            if (preAlarmSeconds < 0) preAlarmSeconds = 0;
        } else if (arg == "--time" || arg == "-t") {
            showLiveTime = true;
        } else if (arg == "--daily" || arg == "-d") {
            useDailyTimes = true;

            while (i + 1 < argc && argv[i + 1][0] != '-') {
                string timeStr = argv[i + 1]; // erst schauen, noch nicht erhöhen
                int h = 0, m = 0, s = 0;
                int parsed = sscanf(timeStr.c_str(), "%d:%d:%d", &h, &m, &s);
                if (parsed < 2) {
                    break; // kein Zeitformat, äußere Schleife übernimmt es (etwa Sounddatei)
                }
                ++i; // jetzt erhöhen

                if (parsed == 2) s = 0;
                dailyTimes.emplace_back(h, m, s);
            }

            if (dailyTimes.empty()) {
                cout << t(Str::ERROR_NO_DAILY_TIMES) << "\n";
                return 1;
            }

            ms = millisecondsUntilNextDailyTime(dailyTimes);
            if (ms <= 0) ms = 1000;

            loop = true;
            maxLoops = -1; // I seek eternal fire

        } else if ((arg == "--lang" || arg == "-la") && i + 1 < argc) {
            ++i; // bereits im Vor-Durchlauf verarbeitet

        } else if (arg == "--version" || arg == "-v") {
        cout << PRG_VERSION << "\n";
        return 0;
        } else if (arg == "--help" || arg == "-h") {
            cout << t(Str::USAGE_HEADER);
            return 0;
        } else if (arg[0] == '-') { // Falls Parameter mit "-" falsch eingegeben wurde. Muss am Ende aller --Parameter stehen.
            char buf[256];
            snprintf(buf, sizeof(buf), t(Str::ERROR_UNKNOWN_OPTION), arg.c_str());
            cout << buf << "\n";
            return 1;
        } else if (!useAtTime) {
            long long possible = parseTime(arg);
            if (possible > 0) {
                ms = possible;
            } else if (isStandaloneUnit(arg)) {
                char buf[256];
                snprintf(buf, sizeof(buf), t(Str::ERROR_DETACHED_UNIT), arg.c_str());
                cout << buf << "\n";
                return 1;
            } else if (soundFile.empty()) {
                soundFile = arg;
            }
        } else if (soundFile.empty()){
            soundFile = arg;
        }
    }

    if (!useAtTime && !useDailyTimes && !showLiveTime && ms <= 0) {
        cout << t(Str::ERROR_NO_TIME) << "\n";
        return 1;
    }

    if (ms > MAX_MS) ms = MAX_MS;

    // Bildschirrmschoner erlauben bzw. unterdrücken
    if (noSleep){
        preventSleep(true);
    }

    if (!showLiveTime){ // Wenn nur die Zeit angezeigt wird, Folgendes weglassen
        char buf[256];

        snprintf(buf, sizeof(buf), t(Str::STARTED), PRG_VERSION);
        cout << buf;

        if (useAtTime) {
            char buf[256];
            if (useAtDateTime) {
                snprintf(buf, sizeof(buf), t(Str::TIMER_AT_DATETIME),
                         atYear, atMonth, atDay, atHour, atMinute, atSecond);
            } else {
                snprintf(buf, sizeof(buf), t(Str::TIMER_AT_TIME),
                         atHour, atMinute, atSecond);
            }
            cout << buf;

            // Prüfen ob Ziel morgen liegt (prima bei reiner Uhrzeit oder nahem Datum)
            auto atWallTarget = chrono::system_clock::now()
                                + chrono::milliseconds(ms);
            time_t atTargetT = chrono::system_clock::to_time_t(atWallTarget);
            if (isTargetTomorrow(atTargetT))
                cout << t(Str::TOMORROW_SUFFIX);

        } else {
            // hier Umrechnung in passende Einheiten
            string timerStr = formatVerbleibend(ms / 1000); // ms -> Sekunden

            if (!useDailyTimes){
                char buf[256];
                snprintf(buf, sizeof(buf), t(Str::TIMER_COUNTER), timerStr.c_str());
                cout << buf;

                // absolute Zielzeit berechnen und anhängen
                auto targetWall = chrono::system_clock::now() + chrono::milliseconds(ms);
                time_t targetT = chrono::system_clock::to_time_t(targetWall);
                tm targetTm{};
                localtime_s(&targetTm, &targetT);
                char tbuf[64];
                snprintf(tbuf, sizeof(tbuf), t(Str::TIMER_TARGET),
                         targetTm.tm_hour, targetTm.tm_min, targetTm.tm_sec);
                cout << tbuf;
            } else {
                cout << t(Str::TIMER_DAILY);
            }
        }
        if (asyncSound) {
            cout << t(Str::ASYNC_SUFFIX);
        }
        cout << "\n";
    }

    // Schleife Direktanzeige von Zeit und Datum
    if (showLiveTime) {
        system("cls"); // Konsole bereinigen, damit nur die Zeit da steht.
        while (true) {
            auto now = chrono::system_clock::now();
            time_t tnow = chrono::system_clock::to_time_t(now);
            tm local{};
            localtime_s(&local, &tnow);

            cout << "\r" << put_time(&local, "%Y-%m-%d %H:%M:%S") << flush;

            // Berechne Millisekunden bis zur nächsten Sekunde
            auto next = chrono::time_point_cast<chrono::seconds>(now) + chrono::seconds(1);
            this_thread::sleep_until(next);
        }
        return 0; // Programm wird über Strg+C beendet
    }


    // Die normale Timer-Schleife
    do {
        if (loop) ++loopCount;

        // Für --daily und --at: absoluten Wanduhr-Zielzeitpunkt frisch bestimmen
        chrono::system_clock::time_point wallTarget;
        if (useDailyTimes) {
            wallTarget = nextDailyTarget(dailyTimes);
        } else if (useAtTime) {
            long long nextMs;
            if (useAtDateTime && loopCount > 1) {
                // Jedes Jahr am selben Kalenderdatum zur selben Uhrzeit wiederholen.
                // Sonderfall 29. Feb: mktime normalisiert auf 1. März in Nicht-Schaltjahren.
                atYear += 1;
                nextMs = millisecondsUntilDateTime(atYear, atMonth, atDay, atHour, atMinute, atSecond);
            } else {
                nextMs = useAtDateTime
                             ? millisecondsUntilDateTime(atYear, atMonth, atDay, atHour, atMinute, atSecond)
                             : millisecondsUntilTime(atHour, atMinute, atSecond);
            }
            if (nextMs == 0) { cout << t(Str::ERROR_NEXT_TIME); return 1; }
            ms = nextMs;
            if (ms > MAX_MS) ms = MAX_MS;
        }

        long long totalMsThisRound = useDailyTimes
                                         ? chrono::duration_cast<chrono::milliseconds>(
                                               wallTarget - chrono::system_clock::now()).count()
                                         : ms;

        // Wanduhr-Ziel als time_t für Tomorrow-Anzeige
        time_t wallTargetT = 0;
        if (useDailyTimes) {
            wallTargetT = chrono::system_clock::to_time_t(wallTarget);
        } else if (useAtTime) {
            wallTargetT = chrono::system_clock::to_time_t(
                chrono::system_clock::now()
                + chrono::milliseconds(totalMsThisRound));
        }

        auto start = chrono::steady_clock::now();
        auto end = start + chrono::milliseconds(totalMsThisRound);

        long long lastVerbleibendSec = -1;

        while (true) {
            auto nowSteady = chrono::steady_clock::now();
            auto nowWall   = chrono::system_clock::now();

            bool done = false;
            long long verbleibendMs = 0;

            if (useDailyTimes) {
                verbleibendMs = chrono::duration_cast<chrono::milliseconds>(
                                    wallTarget - nowWall).count();
                if (verbleibendMs <= 0) done = true;
            } else {
                verbleibendMs = chrono::duration_cast<chrono::milliseconds>(
                                    end - nowSteady).count();
                if (nowSteady >= end) done = true;
            }

            if (done) break;
            if (verbleibendMs < 0) verbleibendMs = 0;

            long long verbleibendSec = (verbleibendMs + 999) / 1000;

            if (verbleibendSec != lastVerbleibendSec) {
                lastVerbleibendSec = verbleibendSec;

                // PRE-ALARM
                if (preAlarmSeconds > 0 &&
                    verbleibendSec > 0 &&
                    verbleibendSec <= preAlarmSeconds)
                {
                    Beep(1200, 80);
                }

                long long totalMs = totalMsThisRound;

                long long elapsedMs = totalMs - verbleibendMs;
                if (elapsedMs < 0) elapsedMs = 0;
                long double fraction = (totalMs > 0)
                                           ? (static_cast<long double>(elapsedMs) / static_cast<long double>(totalMs))
                                           : 1.0L;
                if (fraction < 0.0L) fraction = 0.0L;
                if (fraction > 1.0L) fraction = 1.0L;
                int filled = static_cast<int>(fraction * barWidth);
                if (filled > barWidth) filled = barWidth;

                string verbleibendStr = formatVerbleibend(verbleibendSec);

                SetConsoleTitleA(("Teefax - " + verbleibendStr).c_str()); // Zeit im Fenstertitel anzeigen

                cout << "\r";
                char buf[128];
                if (loop) {
                    snprintf(buf, sizeof(buf), t(Str::LOOP_PREFIX), loopCount);
                    cout << buf;
                }
                snprintf(buf, sizeof(buf), t(Str::REMAINING), verbleibendStr.c_str());
                cout << buf;
                // Tomorrow-Suffix, verschwindet automatisch nach Mitternacht
                if (wallTargetT != 0 && isTargetTomorrow(wallTargetT))
                    cout << t(Str::TOMORROW_SUFFIX);
                cout << " [";

                for (int i = 0; i < barWidth; ++i) cout << (i < filled ? '#' : '-');
                cout << "]   " << flush;
            }

            // Bis zur nächsten Sekundengrenze schlafen:
            // verbleibendMs % 1000 ergibt genau die Ms, die in der aktuellen
            // Anzeigesekunde noch verbleiben. Anzeige springt synchron zur Uhr.
            long long sleepMs = verbleibendMs % 1000;
            if (sleepMs == 0) sleepMs = 1000;          // exakt auf Grenze: volle Sekunde
            if (sleepMs > verbleibendMs) sleepMs = verbleibendMs; // nicht überschießen
            if (sleepMs < 1) sleepMs = 1;

            this_thread::sleep_for(chrono::milliseconds(sleepMs));
        }

        cout << "\r";
        {
            char buf[128];
            if (loop) {
                snprintf(buf, sizeof(buf), t(Str::LOOP_PREFIX), loopCount);
                cout << buf;
            }
            snprintf(buf, sizeof(buf), t(Str::REMAINING), "00:00");
            cout << buf << " [";
        }
        for (int i = 0; i < barWidth; ++i) cout << '#';
        cout << "]   " << flush;

        if (!mute) {
            for (int r = 0; r < alarmRepeat; ++r) {
                if (!soundFile.empty()) {
                    try {
                        fs::path p(soundFile);
                        if (!fs::exists(p) || !fs::is_regular_file(p)) {
                            char buf[512];
                            snprintf(buf, sizeof(buf), t(Str::AUDIO_NOT_FOUND), soundFile.c_str());
                            cout << buf << "\n";
                        } else {
                            wstring widePath = toWide(soundFile);
                            if (!widePath.empty()) {
                                UINT flags = SND_FILENAME | (asyncSound ? SND_ASYNC : SND_SYNC);
                                PlaySoundW(widePath.c_str(), NULL, flags);
                            } else {
                                char buf[512];
                                snprintf(buf, sizeof(buf), t(Str::AUDIO_PATH_ERROR), soundFile.c_str());
                                cout << buf << "\n";
                            }
                        }
                    } catch (const fs::filesystem_error& e) {
                        cout << "\nDateisystem-Fehler: " << e.what() << "\n";
                    }
                } else {
                    if (asyncSound) {
                        thread([](){
                            PlaySoundA(reinterpret_cast<LPCSTR>(sound_data), NULL, SND_MEMORY | SND_ASYNC);
                        }).detach();
                    } else {
                        PlaySoundA(reinterpret_cast<LPCSTR>(sound_data), NULL, SND_MEMORY | SND_SYNC);
                    }
                }
                if (r < alarmRepeat - 1) this_thread::sleep_for(chrono::seconds(alarmInterval));
            }
        }

        // Nächste Wartezeit berechnen (nur noch für --daily nötig, --at läuft oben)
        if (useDailyTimes) {
            wallTarget = nextDailyTarget(dailyTimes);
            ms = chrono::duration_cast<chrono::milliseconds>(
                     wallTarget - chrono::system_clock::now()).count();
            if (ms <= 0) ms = 1000;
        }

        // Datei öffnen oder Konsolenbefehl ausführen
        if (!cmdArg.empty()) {
            runConsoleCommand(cmdArg);
        }
        if (!openFile.empty()) {
            openFileAfterTimer(openFile);
        }

        // Benachrichtigung, Zeit abgelaufen
        if (showMessage) {
            wstring notifyText = toWide(t(Str::NOTIFY_MSG));
            if (!customMsg.empty())
                notifyText += L"\n\n" + toWide(customMsg); // benutzerdefinierte Notiz anhängen
            showNotification(
                toWide(t(Str::NOTIFY_TITLE)),
                notifyText
                );
        }

    } while (loop && (maxLoops == -1 || loopCount < maxLoops));
    cout << t(Str::TIMER_ENDED);

    //Bildschirmschoner wieder erlauben
    if (noSleep){
        preventSleep(false);
    }

    SetConsoleTitleA("Teefax"); // Fenstertitel zurücksetzen

    return 0;
}