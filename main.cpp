#include <iostream>
#include "i18n.h" // Übersetzungen
#include <chrono>
#include <thread>
#include <string>
#include <cmath>
#include <ctime>
#include <windows.h>
#include <mmsystem.h>
#include <filesystem>
#include <limits>
#include <cctype>
#include <atomic>
#include "sound_array.h" // Signalton
#include <vector> // Für täglichen Alarm
#include <sstream> // Für --every Parsing/Formatierung
#include <conio.h> // _kbhit(), _getch() für Stoppuhr-Pause
#include <algorithm>


#pragma comment(lib, "winmm.lib")

using namespace std;

namespace fs = std::filesystem;

// Globale Markierung, ob timeBeginPeriod(1) gesetzt wurde (für Ctrl-C Handler)
static atomic<bool> g_timePeriodSet{false};

// Ursprünglicher Konsolenmodus, wird beim Start gespeichert und beim Beenden wiederhergestellt
static DWORD g_originalConsoleMode = 0;
static bool  g_consoleModeChanged  = false;

// Maximale Millisekunden (wir nutzen die maximale long long, etwas konservativ geclamped)
constexpr long long MAX_MS = std::numeric_limits<long long>::max() / 4;

// Ctrl-C / Console Event Handler: versucht, timeEndPeriod zurückzusetzen
BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (g_timePeriodSet.load()) {
        timeEndPeriod(1);
        g_timePeriodSet.store(false);
    }
    if (g_consoleModeChanged) {
        SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), g_originalConsoleMode);
        g_consoleModeChanged = false;
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
// cp = Codepage (CP_UTF8 für Literal-Strings aus t(); CP_ACP für argv-Werte auf Windows)
wstring toWide(const string& str, UINT cp = CP_UTF8) {
    if (str.empty()) return wstring();
    int size_needed = MultiByteToWideChar(cp, 0, str.c_str(), -1, NULL, 0);
    if (size_needed == 0) return wstring();
    wstring wstr;
    wstr.resize(size_needed);
    int rc = MultiByteToWideChar(cp, 0, str.c_str(), -1, &wstr[0], size_needed);
    if (rc == 0) return wstring();
    if (!wstr.empty() && wstr.back() == L'\0') wstr.pop_back();
    return wstr;
}

// Konvertierung fuer argv-Strings: Windows uebergibt argv in der ANSI-Systemcodepage (CP_ACP),
// nicht in UTF-8. Daher separate Variante fuer alle Werte, die der Nutzer eingibt.
inline wstring toWideArgv(const string& str) {
    return toWide(str, CP_ACP);
}

// Rueckkonvertierung fuer die Konsolenausgabe: wstring -> string in der aktuellen
// Konsolenausgabe-Codepage (GetConsoleOutputCP), damit Umlaute korrekt erscheinen.
// CP_ACP (Eingabe) != GetConsoleOutputCP() (Ausgabe, z.B. CP_850 auf deutschen Systemen).
string toConsole(const wstring& wstr) {
    if (wstr.empty()) return string();
    UINT cp = GetConsoleOutputCP();
    int size = WideCharToMultiByte(cp, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    if (size == 0) return string();
    string str(size, '\0');
    WideCharToMultiByte(cp, 0, wstr.c_str(), -1, &str[0], size, NULL, NULL);
    if (!str.empty() && str.back() == '\0') str.pop_back();
    return str;
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


// Gibt false zurueck, wenn die Datei nicht geoeffnet werden konnte (z. B. nicht gefunden),
// damit der Aufrufer bei Bedarf die Schleife abbrechen kann (wie --focus).
// Fehlermeldungen werden mit \r auf Spalte 0 positioniert: der naechste Schleifendurchlauf
// ueberschreibt sie per \r wieder; beim letzten Durchlauf bleibt die Meldung stehen.
// Alle i18n-Fehlertexte beginnen mit \n (historisch). Das wird hier uebersprungen.
bool openFileAfterTimer(const string& filePath) {
    // Fehlermeldung überschreibt den Balken per \r; trailing spaces loeschen
    // Balkenreste, die laenger als die Fehlermeldung sind.
    auto printErr = [](const char* buf) {
        const char* msg = (buf[0] == '\n') ? buf + 1 : buf;
        cout << "\r" << msg << "                                        " << flush;
    };

    bool isUrl = filePath.rfind("http://", 0) == 0 ||
                 filePath.rfind("https://", 0) == 0;

    if (!isUrl) {
        try {
            if (!fs::exists(fs::path(filePath))) {
                char buf[512];
                snprintf(buf, sizeof(buf), t(Str::FILE_NOT_FOUND),
                         toConsole(toWideArgv(filePath)).c_str());
                printErr(buf);
                return false;
            }
        } catch (const fs::filesystem_error& e) {
            char buf[512];
            snprintf(buf, sizeof(buf), t(Str::FILE_SYSTEM_ERROR), e.what());
            printErr(buf);
            return false;
        }
    }

    wstring wfile = toWideArgv(filePath);
    if (wfile.empty()) {
        char buf[512];
        snprintf(buf, sizeof(buf), t(Str::ERROR_FILE_CONVERSION),
                 toConsole(toWideArgv(filePath)).c_str());
        printErr(buf);
        return false;
    }
    string display = toConsole(wfile);
    HINSTANCE res = ShellExecuteW(NULL, L"open", wfile.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)res <= 32) {
        char buf[256];
        snprintf(buf, sizeof(buf), t(Str::FILE_ERROR), (INT_PTR)res, display.c_str());
        printErr(buf);
        return false;
    }
    return true;
}

// CMD_STARTED wird vor dem Start ausgegeben, damit die Ausgabe des Prozesses
// geordnet darunter erscheint. WaitForSingleObject stellt sicher, dass der
// Kindprozess fertig ist, bevor die naechste Schleifenrunde oder TIMER_ENDED
// beginnt. Verhindert Zeileninterleaving und doppelte Durchlauf-Anzeigen.
// Hinweis: Langlaeufer (z. B. "notepad.exe" ohne "start") blockieren teefax
// bis zum Beenden des Programms. Fuer solche Faelle "start" vorschalten.
void runConsoleCommand(const string& command) {
    if (command.empty()) {
        cout << "\n" << t(Str::NO_COMMAND) << "\n";
        return;
    }

    wstring wcommand = toWideArgv(command);
    if (wcommand.empty()) {
        char buf[512];
        snprintf(buf, sizeof(buf), t(Str::ERROR_CMD_CONVERSION),
                 toConsole(toWideArgv(command)).c_str());
        cout << "\n" << buf << "\n";
        return;
    }

    string display = toConsole(wcommand);

    // CMD_STARTED vor dem Prozessstart: Ausgabe erscheint geordnet darunter
    {
        char buf[512];
        snprintf(buf, sizeof(buf), t(Str::CMD_STARTED), display.c_str());
        cout << buf << "\n" << flush;
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
        snprintf(buf, sizeof(buf), t(Str::CMD_ERROR), (int)err, display.c_str());
        cout << buf << "\n";
        return;
    }

    // Warten bis Prozess beendet: verhindert Ausgabe-Interleaving mit dem
    // naechsten Balken. Langlaeufer blockieren teefax, ggf. "start" nutzen.
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

// Fenster anhand eines Teilstrings im Titel in den Vordergrund holen
struct FindWindowData {
    wstring titlePart;         // Suchbegriff (lowercase)
    HWND    excludeHwnd;       // eigenes Konsolenfenster, wird ignoriert
    HWND    result = nullptr;
};

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    auto* data = reinterpret_cast<FindWindowData*>(lParam);

    if (hwnd == data->excludeHwnd) return TRUE; // eigenes Fenster ueberspringen
    if (!IsWindowVisible(hwnd))    return TRUE;

    wchar_t buf[512];
    if (GetWindowTextW(hwnd, buf, 512) == 0) return TRUE;

    wstring titleLow(buf);
    transform(titleLow.begin(), titleLow.end(), titleLow.begin(),
              [](wchar_t c){ return static_cast<wchar_t>(towlower(c)); });

    if (titleLow.find(data->titlePart) != wstring::npos) {
        data->result = hwnd;
        return FALSE; // Enumeration stoppen
    }
    return TRUE;
}

// Sucht ein sichtbares Fenster anhand eines Teilstrings, ohne Seiteneffekte.
static HWND findWindowByTitle(const string& titleStr)
{
    wstring part = toWideArgv(titleStr);
    transform(part.begin(), part.end(), part.begin(),
              [](wchar_t c){ return static_cast<wchar_t>(towlower(c)); });

    FindWindowData data;
    data.titlePart   = part;
    data.excludeHwnd = GetConsoleWindow();
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));
    return data.result;
}

bool bringWindowToFront(const string& titleStr)
{
    HWND hwnd = findWindowByTitle(titleStr);
    if (!hwnd)
        return false;

    // Minimiert? -> Wiederherstellen
    if (IsIconic(hwnd))
        ShowWindow(hwnd, SW_RESTORE);

    // In den Vordergrund bringen
    SetForegroundWindow(hwnd);
    BringWindowToTop(hwnd);
    SetActiveWindow(hwnd);
    return true;
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

// ── --every: Wochentag- oder Monatstag-Wiederholung ──────────────────

struct EverySpec {
    enum class Type { Weekday, MonthDay } type = Type::Weekday;
    vector<int> days;   // Wochentage: 0=So…6=Sa  |  Monatstage: 1-31
    int hour = 0, minute = 0, second = 0;
};

// Gibt den tm_wday-Wert (0-6) für einen Tagnamen zurück, oder -1
int parseWeekday(const string& s) {
    string u;
    for (char c : s) u += static_cast<char>(tolower(static_cast<unsigned char>(c)));
    if (u == "sun" || u == "sunday")    return 0;
    if (u == "mon" || u == "monday")    return 1;
    if (u == "tue" || u == "tuesday")   return 2;
    if (u == "wed" || u == "wednesday") return 3;
    if (u == "thu" || u == "thursday")  return 4;
    if (u == "fri" || u == "friday")    return 5;
    if (u == "sat" || u == "saturday")  return 6;
    return -1;
}

// Kommaseparierte Tag-Liste parsen: "mon,wed,fri" oder "1,15"
EverySpec parseEverySpec(const string& daysStr, int h, int m, int s) {
    EverySpec spec;
    spec.hour = h; spec.minute = m; spec.second = s;

    istringstream ss(daysStr);
    string token;
    while (getline(ss, token, ',')) {
        if (token.empty()) continue;
        int wd = parseWeekday(token);
        if (wd >= 0) {
            spec.type = EverySpec::Type::Weekday;
            spec.days.push_back(wd);
        } else {
            int day = safeStoi(token, -1);
            if (day >= 1 && day <= 31) {
                spec.type = EverySpec::Type::MonthDay;
                spec.days.push_back(day);
            }
        }
    }
    return spec;
}

// Tage als lesbaren String ausgeben: "Mon,Wed,Fri" oder "1.,15."
string formatEveryDays(const EverySpec& spec) {
    static const char* wdNames[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    ostringstream ss;
    for (size_t i = 0; i < spec.days.size(); ++i) {
        if (i > 0) ss << ",";
        if (spec.type == EverySpec::Type::Weekday)
            ss << wdNames[spec.days[i]];
        else
            ss << spec.days[i] << ".";
    }
    return ss.str();
}

// Nächsten Zielzeitpunkt für --every berechnen (bis zu 400 Tage voraus)
chrono::system_clock::time_point nextEveryTarget(const EverySpec& spec) {
    using namespace chrono;

    auto now   = system_clock::now();
    time_t tnow = system_clock::to_time_t(now);
    tm base{};
    localtime_s(&base, &tnow);

    for (int offset = 0; offset <= 400; ++offset) {
        tm candidate  = base;
        candidate.tm_mday  += offset;
        candidate.tm_hour   = spec.hour;
        candidate.tm_min    = spec.minute;
        candidate.tm_sec    = spec.second;
        candidate.tm_isdst  = -1;

        time_t tt = mktime(&candidate);
        if (tt == -1) continue;

        auto target = system_clock::from_time_t(tt);
        if (target <= now) continue;   // liegt in der Vergangenheit

        bool match = false;
        if (spec.type == EverySpec::Type::Weekday) {
            for (int d : spec.days)
                if (d == candidate.tm_wday) { match = true; break; }
        } else {
            for (int d : spec.days)
                if (d == candidate.tm_mday) { match = true; break; }
        }

        if (match) return target;
    }

    return now + hours(24); // Fallback, sollte nie eintreten
}

// ── Konfigurationsdatei (CLI-Format) ─────────────────────────────────
// Tokenisiert eine Konfigurationszeile, respektiert einfache und doppelte
// Anführungszeichen und bricht bei '#' ab (Kommentar).
static vector<string> tokenizeConfigLine(const string& line) {
    vector<string> tokens;
    string cur;
    bool inQuote = false;
    char quoteChar = 0;
    for (char c : line) {
        if (inQuote) {
            if (c == quoteChar) inQuote = false;
            else cur += c;
        } else if (c == '"' || c == '\'') {
            inQuote = true;
            quoteChar = c;
        } else if (c == '#') {
            break; // Rest der Zeile ist Kommentar
        } else if (isspace(static_cast<unsigned char>(c))) {
            if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

// Liest teefax.ini aus demselben Verzeichnis wie die .exe und hängt
// alle geparsten Tokens an 'out' an. Fehlt die Datei, passiert nichts.
static void loadConfigArgs(vector<string>& out) {
    wchar_t exeBuf[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exeBuf, MAX_PATH) == 0) return;

    wstring iniPath(exeBuf);
    size_t slash = iniPath.rfind(L'\\');
    if (slash == wstring::npos) return;
    iniPath = iniPath.substr(0, slash + 1) + L"teefax.ini";

    FILE* f = _wfopen(iniPath.c_str(), L"r");
    if (!f) return;

    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {
        string line(buf);
        // Zeilenende abschneiden
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        // Führende Leerzeichen / Tabs überspringen
        size_t start = line.find_first_not_of(" \t");
        if (start == string::npos) continue;
        line = line.substr(start);
        if (line[0] == '#') continue; // Kommentarzeile
        if (line.rfind("macro ", 0) == 0) continue; // Makro-Definition, nicht als Argument laden
        for (auto& tok : tokenizeConfigLine(line))
            out.push_back(tok);
    }
    fclose(f);
}

// ── Makro-System ──────────────────────────────────────────────────────

// Pfad zur teefax.ini ermitteln (wird von mehreren Makro-Funktionen benoetigt)
static wstring getIniPath() {
    wchar_t exeBuf[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exeBuf, MAX_PATH) == 0) return L"";
    wstring p(exeBuf);
    size_t slash = p.rfind(L'\\');
    if (slash == wstring::npos) return L"";
    return p.substr(0, slash + 1) + L"teefax.ini";
}

// Alle Makros aus der ini laden: "macro <name> = <args>" -> map
static unordered_map<string,string> loadMacros() {
    unordered_map<string,string> macros;
    wstring iniPath = getIniPath();
    if (iniPath.empty()) return macros;

    FILE* f = _wfopen(iniPath.c_str(), L"r");
    if (!f) return macros;

    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {
        string line(buf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        size_t start = line.find_first_not_of(" \t");
        if (start == string::npos) continue;
        line = line.substr(start);
        if (line[0] == '#') continue;

        // Erwartet: "macro <name> = <args>"
        if (line.rfind("macro ", 0) != 0) continue;
        size_t nameStart = 6; // nach "macro "
        size_t eq = line.find('=', nameStart);
        if (eq == string::npos) continue;

        string name = line.substr(nameStart, eq - nameStart);
        size_t ns = name.find_first_not_of(" \t");
        size_t ne = name.find_last_not_of(" \t");
        if (ns == string::npos) continue;
        name = name.substr(ns, ne - ns + 1);

        string value = line.substr(eq + 1);
        size_t vs = value.find_first_not_of(" \t");
        if (vs != string::npos) value = value.substr(vs);
        size_t ve = value.find_last_not_of(" \t");
        if (ve != string::npos) value = value.substr(0, ve + 1);

        if (!name.empty()) macros[name] = value;
    }
    fclose(f);
    return macros;
}

// Reservierte Namen: alle bekannten Flags und Kurzformen
static bool isMacroNameReserved(const string& name) {
    static const vector<string> reserved = {
        "--mute","-m","--loop","-l","--nomsg","--msg","--alarm-repeat","-ar",
        "--alarm-interval","-ai","--async","-as","--at","-a","--until",
        "--open","-o","--cmd","-c","--focus","-f","--prealarm","-pa",
        "--time","-t","--stopwatch","-sw","--daily","-d","--every","-e",
        "--nosleep","-ns","--lang","-la","--version","-v","--help","-h",
        "--macro"
    };
    for (const auto& r : reserved)
        if (name == r) return true;
    return false;
}

// Makroname darf nur Buchstaben und Ziffern enthalten
static bool isMacroNameValid(const string& name) {
    if (name.empty()) return false;
    for (char c : name)
        if (!isalnum(static_cast<unsigned char>(c))) return false;
    return true;
}

// Makro in ini schreiben (neu oder ueberschreiben).
// Bestehende "macro <name> = ..."-Zeile wird ersetzt, sonst ans Ende angehaengt.
static void saveMacroToIni(const string& name, const string& args) {
    wstring iniPath = getIniPath();
    if (iniPath.empty()) return;

    vector<string> lines;
    FILE* f = _wfopen(iniPath.c_str(), L"r");
    if (f) {
        char buf[1024];
        while (fgets(buf, sizeof(buf), f)) {
            string line(buf);
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.pop_back();
            lines.push_back(line);
        }
        fclose(f);
    }

    string entry  = "macro " + name + " = " + args;
    string prefix = "macro " + name + " ";

    bool replaced = false;
    for (auto& line : lines) {
        string trimmed = line;
        size_t s = trimmed.find_first_not_of(" \t");
        if (s != string::npos) trimmed = trimmed.substr(s);
        if (trimmed.rfind(prefix, 0) == 0 && trimmed.find('=') != string::npos) {
            line = entry;
            replaced = true;
            break;
        }
    }
    if (!replaced) lines.push_back(entry);

    FILE* fw = _wfopen(iniPath.c_str(), L"w");
    if (!fw) return;
    for (const auto& line : lines)
        fprintf(fw, "%s\n", line.c_str());
    fclose(fw);
}

// Makro aus ini entfernen; gibt true zurueck wenn gefunden und entfernt
static bool removeMacroFromIni(const string& name) {
    wstring iniPath = getIniPath();
    if (iniPath.empty()) return false;

    vector<string> lines;
    FILE* f = _wfopen(iniPath.c_str(), L"r");
    if (!f) return false;
    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {
        string line(buf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        lines.push_back(line);
    }
    fclose(f);

    string prefix = "macro " + name + " ";
    bool found = false;
    vector<string> kept;
    for (const auto& line : lines) {
        string trimmed = line;
        size_t s = trimmed.find_first_not_of(" \t");
        if (s != string::npos) trimmed = trimmed.substr(s);
        if (!found && trimmed.rfind(prefix, 0) == 0 && trimmed.find('=') != string::npos) {
            found = true; // diese Zeile weglassen
        } else {
            kept.push_back(line);
        }
    }
    if (!found) return false;

    FILE* fw = _wfopen(iniPath.c_str(), L"w");
    if (!fw) return false;
    for (const auto& line : kept)
        fprintf(fw, "%s\n", line.c_str());
    fclose(fw);
    return true;
}

// ── Ende Makro-System ─────────────────────────────────────────────────

// Erkennt, ob Teefax aus einer bestehenden Konsole aufgerufen wurde
// oder ob Windows selbst die Konsole erstellt hat (= Doppelklick)
// Erzeugt einen WAV-Puffer mit 1 Sekunde Stille (22050 Hz, 16-bit, mono).
// Wird einmalig aufgebaut. Zeiger bleibt fuer die Lebensdauer des Programms gueltig.
static const vector<uint8_t>& silentWav() {
    static vector<uint8_t> wav = [](){
        constexpr uint32_t RATE  = 22050;
        constexpr uint32_t DSIZE = RATE * 2; // 1s, 16-bit mono = 44100 Bytes
        vector<uint8_t> w(44 + DSIZE, 0);
        auto w16 = [&](size_t o, uint16_t v){ memcpy(w.data()+o, &v, 2); };
        auto w32 = [&](size_t o, uint32_t v){ memcpy(w.data()+o, &v, 4); };
        memcpy(w.data()+0,  "RIFF", 4); w32(4,  36 + DSIZE);
        memcpy(w.data()+8,  "WAVE", 4);
        memcpy(w.data()+12, "fmt ", 4); w32(16, 16);
        w16(20, 1); w16(22, 1); w32(24, RATE); w32(28, RATE*2); w16(32, 2); w16(34, 16);
        memcpy(w.data()+36, "data", 4); w32(40, DSIZE);
        // w[44..] = 0 (Stille, bereits initialisiert)
        return w;
    }();
    return wav;
}

// Generiert einen WAV-Puffer fuer den gesamten Voralarm-Zeitraum:
// 'prewarmMs' Millisekunden Stille (BT-Aufwaermung), danach 'count' Beeps
// im Sekundentakt (880 Hz, 100 ms Ton + 900 ms Stille pro Zyklus).
// Kein SND_LOOP noetig. Der Puffer endet nach dem letzten Beep von selbst.
// Kontinuierlicher Audio-Stream: kein Soundwechsel, kein BT-Gap beim ersten Beep.
// Rueckgabe: leerer Vektor wenn count <= 0.
static vector<uint8_t> buildPreAlarmWav(int count, int prewarmMs = 2000) {
    if (count <= 0) return {};

    constexpr uint32_t RATE         = 22050;
    constexpr uint32_t BEEP_SAMPLES = RATE * 100 / 1000; // 100 ms = 2205 Samples
    constexpr float    FREQ         = 880.0f;
    constexpr float    PI2          = 6.28318530f;

    const uint32_t prewarmSamples = static_cast<uint32_t>(prewarmMs) * RATE / 1000;
    const uint32_t cycleSamples   = RATE;                              // 1 s pro Beep-Zyklus
    const uint32_t totalSamples   = prewarmSamples + static_cast<uint32_t>(count) * cycleSamples;
    const uint32_t DSIZE          = totalSamples * 2;                  // 16-bit mono

    vector<uint8_t> w(44 + DSIZE, 0);
    auto w16 = [&](size_t o, uint16_t v){ memcpy(w.data()+o, &v, 2); };
    auto w32 = [&](size_t o, uint32_t v){ memcpy(w.data()+o, &v, 4); };

    memcpy(w.data()+0,  "RIFF", 4); w32(4,  36 + DSIZE);
    memcpy(w.data()+8,  "WAVE", 4);
    memcpy(w.data()+12, "fmt ", 4); w32(16, 16);
    w16(20, 1); w16(22, 1); w32(24, RATE); w32(28, RATE*2); w16(32, 2); w16(34, 16);
    memcpy(w.data()+36, "data", 4); w32(40, DSIZE);

    auto* samples = reinterpret_cast<int16_t*>(w.data() + 44);
    const uint32_t FADE = BEEP_SAMPLES / 10;

    for (int b = 0; b < count; ++b) {
        uint32_t offset = prewarmSamples + static_cast<uint32_t>(b) * cycleSamples;
        for (uint32_t i = 0; i < BEEP_SAMPLES; ++i) {
            float env = 1.0f;
            if (i < FADE)                     env = static_cast<float>(i)                / static_cast<float>(FADE);
            else if (i > BEEP_SAMPLES - FADE) env = static_cast<float>(BEEP_SAMPLES - i) / static_cast<float>(FADE);
            float s = env * 0.55f * 32767.0f * sinf(PI2 * FREQ * static_cast<float>(i) / static_cast<float>(RATE));
            samples[offset + i] = static_cast<int16_t>(s);
        }
        // Stille zwischen den Beeps ist bereits 0-initialisiert
    }
    return w;
}

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
    string focusWindow; // Fenstertitel für --focus
    bool showMessage = true; // Standard: MessageBox ist aktiv
    bool noSleep = false; // Bildschirmschoner & Standby nicht unterdrücken
    int preAlarmSeconds = 0; // Sekunden vor Schluss, in denen sekündlich gepiept wird
    const int barWidth = 30;
    long long loopCount = 0;
    bool showLiveTime = false; // Für die direkte Zeitanzeige, wie der Name schon sagt.
    bool showStopwatch = false; // Stoppuhr-Modus
    vector<tuple<int,int,int>> dailyTimes;
    bool useDailyTimes = false;
    EverySpec everySpec;
    bool useEvery = false;
    string customMsg; // benutzerdefinierte Notiz in Benachrichtigungsfenster am Schluss

    // Kombinierte Argumentliste: Config-Defaults zuerst, dann Kommandozeile.
    // Dadurch können alle CLI-Flags auch in teefax.ini gesetzt werden.
    // Explizite Kommandozeilenargumente überschreiben Config-Werte automatisch
    // (letzter Wert gewinnt bei Flags wie --lang, --alarm-repeat usw.).
    vector<string> args;
    loadConfigArgs(args);
    for (int i = 1; i < argc; ++i)
        args.push_back(argv[i]);

    // ── --macro Verwaltungsbefehle (vor allem anderen auswerten) ─────────
    // Sprache muss dafür schon gesetzt sein, deshalb hier ein Mini-Vorlauf.
    for (int i = 0; i < static_cast<int>(args.size()); ++i) {
        if ((args[i] == "--lang" || args[i] == "-la") && i + 1 < static_cast<int>(args.size())) {
            _putenv_s("TEEFAX_LANG", args[i + 1].c_str());
        }
    }

    for (int i = 0; i < static_cast<int>(args.size()); ++i) {
        if (args[i] != "--macro") continue;

        if (i + 1 >= static_cast<int>(args.size())) {
            cout << t(Str::MACRO_MISSING_NAME) << "\n";
            return 1;
        }

        string subcmd = args[i + 1];

        // ── --macro list ─────────────────────────────────────────────
        if (subcmd == "list") {
            auto macros = loadMacros();
            if (macros.empty()) {
                cout << t(Str::MACRO_LIST_EMPTY) << "\n";
            } else {
                cout << t(Str::MACRO_LIST_HEADER) << "\n";
                for (const auto& kv : macros)
                    cout << "  " << kv.first << " = " << kv.second << "\n";
            }
            return 0;
        }

        // ── --macro remove <name> ────────────────────────────────────
        if (subcmd == "remove") {
            if (i + 2 >= static_cast<int>(args.size())) {
                cout << t(Str::MACRO_MISSING_NAME) << "\n";
                return 1;
            }
            string name = args[i + 2];
            char buf[256];
            if (removeMacroFromIni(name)) {
                snprintf(buf, sizeof(buf), t(Str::MACRO_REMOVED), name.c_str());
                cout << buf << "\n";
            } else {
                snprintf(buf, sizeof(buf), t(Str::MACRO_NOT_FOUND), name.c_str());
                cout << buf << "\n";
                return 1;
            }
            return 0;
        }

        // ── --macro add <name> <args...> ─────────────────────────────
        if (subcmd == "add") {
            if (i + 2 >= static_cast<int>(args.size())) {
                cout << t(Str::MACRO_MISSING_NAME) << "\n";
                return 1;
            }
            string name = args[i + 2];
            char buf[256];

            if (!isMacroNameValid(name)) {
                snprintf(buf, sizeof(buf), t(Str::MACRO_INVALID_NAME), name.c_str());
                cout << buf << "\n";
                return 1;
            }
            if (isMacroNameReserved(name)) {
                snprintf(buf, sizeof(buf), t(Str::MACRO_NAME_RESERVED), name.c_str());
                cout << buf << "\n";
                return 1;
            }
            if (i + 3 >= static_cast<int>(args.size())) {
                cout << t(Str::MACRO_MISSING_ARGS) << "\n";
                return 1;
            }

            // Alle verbleibenden Argumente als Makro-Body zusammensetzen.
            // Quoting-Regeln für die INI-Zeile:
            // - Token enthält Leerzeichen           -> immer quoten
            // - Token folgt auf ein Flag mit Wert-Parameter (--focus, --msg,
            //   --open, --cmd, --at, --lang, --alarm-repeat, --alarm-interval,
            //   --prealarm, --loop, --every, --daily) -> immer quoten,
            //   damit beim späteren Einlesen via tokenizeConfigLine der Wert
            //   als ein Token wiederhergestellt wird.
            static const vector<string> valueFlags = {
                "--focus", "-f", "--msg", "--open", "-o", "--cmd", "-c",
                "--at", "-a", "--until", "--lang", "-la",
                "--alarm-repeat", "-ar", "--alarm-interval", "-ai",
                "--prealarm", "-pa", "--loop", "-l",
                "--every", "-e", "--daily", "-d",
            };
            string macroArgs;
            bool nextNeedsQuotes = false;
            for (int j = i + 3; j < static_cast<int>(args.size()); ++j) {
                if (j > i + 3) macroArgs += " ";
                const string& tok = args[j];
                bool needsQuotes = nextNeedsQuotes || tok.find(' ') != string::npos;
                // Prüfen ob dieses Token selbst eine Option mit Argument ist
                nextNeedsQuotes = false;
                for (const auto& vf : valueFlags) {
                    if (tok == vf) { nextNeedsQuotes = true; break; }
                }
                if (needsQuotes) macroArgs += '"';
                macroArgs += tok;
                if (needsQuotes) macroArgs += '"';
            }

            // Rückfrage wenn Makro bereits existiert
            auto macros = loadMacros();
            if (macros.count(name)) {
                snprintf(buf, sizeof(buf), t(Str::MACRO_OVERWRITE_PROMPT), name.c_str());
                cout << buf << flush;
                // QuickEdit kurz wiederherstellen damit cin funktioniert
                if (g_consoleModeChanged) {
                    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), g_originalConsoleMode);
                }
                string answer;
                getline(cin, answer);
                if (g_consoleModeChanged) {
                    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
                    DWORD mode = 0;
                    if (GetConsoleMode(hIn, &mode)) {
                        mode &= ~ENABLE_QUICK_EDIT_MODE;
                        mode |=  ENABLE_EXTENDED_FLAGS;
                        SetConsoleMode(hIn, mode);
                    }
                }
                string yesChars = t(Str::MACRO_OVERWRITE_YES);
                if (answer.empty() || yesChars.find(answer[0]) == string::npos)
                    return 0; // abgebrochen
            }

            saveMacroToIni(name, macroArgs);
            snprintf(buf, sizeof(buf), t(Str::MACRO_ADDED), name.c_str());
            cout << buf << "\n";
            return 0;
        }

        // Unbekannter Subbefehl
        cout << t(Str::MACRO_MISSING_NAME) << "\n";
        return 1;
    }

    // ── Makro-Expansion ───────────────────────────────────────────────
    // Wenn das erste Nicht-INI-Argument ein bekannter Makroname ist,
    // werden dessen Tokens an die Argumentliste angehaengt.
    // Nur CLI-Argumente werden geprueft (ab Index args.size() - argc + 1),
    // um zu verhindern, dass INI-Tokens als Makronamen gewertet werden.
    {
        auto macros = loadMacros();
        int cliStart = static_cast<int>(args.size()) - (argc - 1);
        if (cliStart < 0) cliStart = 0;
        for (int i = cliStart; i < static_cast<int>(args.size()); ++i) {
            const string& a = args[i];
            if (a[0] == '-') continue; // Flags ueberspringen
            auto it = macros.find(a);
            if (it != macros.end()) {
                // Makroname durch expandierte Tokens ersetzen
                vector<string> expanded = tokenizeConfigLine(it->second);
                args.erase(args.begin() + i);
                args.insert(args.begin() + i, expanded.begin(), expanded.end());
                break; // nur einmal expandieren
            }
        }
    }

    const int nArgs = static_cast<int>(args.size());

    // Audio priorisieren
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    TimePeriodGuard timeGuard;

    // QuickEdit-Modus deaktivieren: verhindert, dass ein Mausklick ins Konsolenfenster
    // den Timer einfriert (Windows pausiert den Prozess, sobald Text markiert wird).
    // Der Originalzustand wird beim Beenden und bei Strg+C wiederhergestellt.
    {
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(hIn, &mode)) {
            g_originalConsoleMode = mode;
            g_consoleModeChanged  = true;
            mode &= ~ENABLE_QUICK_EDIT_MODE;
            mode |= ENABLE_EXTENDED_FLAGS;
            SetConsoleMode(hIn, mode);
        }
    }

    // Sprache vorab setzen, damit currentLang() korrekt initialisiert wird.
    // Kein break: alle --lang-Vorkommen auswerten, damit CLI-Argumente
    // einen INI-Wert ueberschreiben koennen (letzter Wert gewinnt).
    for (int i = 0; i < nArgs; ++i) {
        const string& a = args[i];
        if ((a == "--lang" || a == "-la") && i + 1 < nArgs) {
            _putenv_s("TEEFAX_LANG", args[i + 1].c_str());
            ++i; // Wert ueberspringen
        }
    }

    if (argc < 2) {
        char buf[256];
        snprintf(buf, sizeof(buf), t(Str::STARTED), PRG_VERSION);
        cout << buf << ".\n\n";
        cout << t(Str::USAGE_HEADER);

        if (!launchedFromExistingConsole()) {
            cout << "\n" << flush;
            system("pause"); // oder: cout << "Druecke eine Taste..."; cin.get();
        }

        return 0;
    }

    // Argument-Parsen (Config-Defaults + Kommandozeile in einem Durchlauf)
    for (int i = 0; i < nArgs; ++i) {
        const string& arg = args[i];

        if (arg == "--nosleep" || arg == "-ns") {
            noSleep = true;
        } else if (arg == "--mute" || arg == "-m") {
            mute = true;
        } else if (arg == "--loop" || arg == "-l") {
            loop = true;
            if (i + 1 < nArgs) {
                int possibleCount = safeStoi(args[i + 1], -1);
                if (possibleCount > 0) {
                    maxLoops = possibleCount;
                    ++i;
                }
            }
        } else if (arg == "--nomsg") {
            showMessage = false;
        } else if (arg == "--msg" && i + 1 < nArgs) {
            customMsg = args[++i];
        } else if ((arg == "--alarm-repeat" || arg == "-ar") && i + 1 < nArgs) {
            alarmRepeat = safeStoi(args[++i], 1);
            if (alarmRepeat < 0) alarmRepeat = 1;
        } else if ((arg == "--alarm-interval" || arg == "-ai") && i + 1 < nArgs) {
            alarmInterval = safeStoi(args[++i], 2);
            if (alarmInterval < 1) alarmInterval = 1;
        } else if (arg == "--async" || arg == "-as") {
            asyncSound = true;
        } else if ((arg == "--at" || arg == "-a" || arg == "--until") && i + 1 < nArgs) {
            string first = args[++i];

            int year, month, day;
            int hour = 0, minute = 0, second = 0;

            // Fall 1: Datum erkannt
            if (sscanf(first.c_str(), "%d-%d-%d", &year, &month, &day) == 3)
            {
                // Prüfen, ob danach eine Uhrzeit folgt (kein weiterer Parameter oder beginnt mit '-')
                if (i + 1 < nArgs && args[i + 1][0] != '-')
                {
                    string timeStr = args[i + 1];

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
                        // Keine gültige Uhrzeit, also Mitternacht verwenden
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
        } else if ((arg == "--open" || arg == "-o") && i + 1 < nArgs) {
            openFile = args[++i];
        } else if ((arg == "--cmd" || arg == "-c") && i + 1 < nArgs) {
            cmdArg = args[++i];
        } else if ((arg == "--focus" || arg == "-f") && i + 1 < nArgs) {
            focusWindow = args[++i];
        } else if ((arg == "--prealarm" || arg == "-pa") && i + 1 < nArgs) { // Sekündliches Piepsen vor Schluss
            preAlarmSeconds = safeStoi(args[++i], 0);
            if (preAlarmSeconds < 0) preAlarmSeconds = 0;
        } else if (arg == "--time" || arg == "-t") {
            showLiveTime = true;
        } else if (arg == "--stopwatch" || arg == "-sw") {
            showStopwatch = true;
        } else if (arg == "--daily" || arg == "-d") {
            useDailyTimes = true;

            while (i + 1 < nArgs && args[i + 1][0] != '-') {
                string timeStr = args[i + 1]; // erst schauen, noch nicht erhöhen
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

        } else if ((arg == "--every" || arg == "-e") && i + 1 < nArgs) {
            string daysStr = args[++i];
            int h = 0, m = 0, s = 0;

            // Optionale Uhrzeit einlesen (sofern kein anderes Argument folgt)
            if (i + 1 < nArgs && args[i + 1][0] != '-') {
                string timeStr = args[i + 1];
                int parsed = sscanf(timeStr.c_str(), "%d:%d:%d", &h, &m, &s);
                if (parsed >= 2) {
                    if (parsed == 2) s = 0;
                    ++i;
                }
                // Kein gültiges Zeitformat, also nicht konsumieren (evtl. Sounddatei)
            }

            everySpec = parseEverySpec(daysStr, h, m, s);
            if (everySpec.days.empty()) {
                char buf[256];
                snprintf(buf, sizeof(buf), t(Str::ERROR_INVALID_EVERY), daysStr.c_str());
                cout << buf << "\n";
                return 1;
            }
            useEvery  = true;
            loop      = true;
            maxLoops  = -1;

            auto target = nextEveryTarget(everySpec);
            ms = chrono::duration_cast<chrono::milliseconds>(
                     target - chrono::system_clock::now()).count();
            if (ms <= 0) ms = 1000;

        } else if ((arg == "--lang" || arg == "-la") && i + 1 < nArgs) {
            ++i; // bereits im Vor-Durchlauf verarbeitet

        } else if (arg == "--macro") {
            // Bereits vor dem Haupt-Parser vollstaendig behandelt (add/remove/list).
            // Hier nur die verbleibenden Tokens ueberspringen, damit kein Fehler entsteht.
            if (i + 1 < nArgs) ++i; // Subbefehl (add/remove/list)
            if (i + 1 < nArgs) ++i; // Name
            if (i + 1 < nArgs) ++i; // evtl. Args (fuer "add")

        } else if (arg == "--version" || arg == "-v") {
            cout << PRG_VERSION << "\n";
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            cout << t(Str::USAGE_HEADER);
            if (!launchedFromExistingConsole()) {
                cout << "\n";
                system("pause");
            }
            return 0;
        } else if (arg[0] == '-') { // Falls Parameter mit "-" falsch eingegeben wurde. Muss am Ende aller --Parameter stehen.
            char buf[256];
            snprintf(buf, sizeof(buf), t(Str::ERROR_UNKNOWN_OPTION), arg.c_str());
            cout << buf << "\n";
            return 1;
        } else if (!useAtTime) {
            long long possible = parseTime(arg);
            if (possible > 0) {
                if (ms > MAX_MS - possible) ms = MAX_MS;
                else ms += possible;
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

    if (!useAtTime && !useDailyTimes && !useEvery && !showLiveTime && !showStopwatch && ms <= 0) {
        cout << t(Str::ERROR_NO_TIME) << "\n";
        return 1;
    }

    if (ms > MAX_MS) ms = MAX_MS;

    // Bildschirrmschoner erlauben bzw. unterdrücken
    if (noSleep){
        preventSleep(true);
    }

    if (!showLiveTime && !showStopwatch){ // Wenn nur die Zeit angezeigt wird oder Stoppuhr läuft, Folgendes weglassen
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

            if (useDailyTimes) {
                cout << t(Str::TIMER_DAILY);
            } else if (useEvery) {
                char buf[256];
                string daysStr = formatEveryDays(everySpec);
                snprintf(buf, sizeof(buf), t(Str::TIMER_EVERY),
                         daysStr.c_str(), everySpec.hour, everySpec.minute, everySpec.second);
                cout << buf;
            } else {
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
            }
        }
        if (asyncSound) {
            cout << t(Str::ASYNC_SUFFIX);
        }
        if (!focusWindow.empty()) {
            char buf[256];
            snprintf(buf, sizeof(buf), t(Str::FOCUS_TARGET),
                     toConsole(toWideArgv(focusWindow)).c_str());
            cout << buf;
        }
        if (!openFile.empty()) {
            char buf[256];
            snprintf(buf, sizeof(buf), t(Str::OPEN_TARGET),
                     toConsole(toWideArgv(openFile)).c_str());
            cout << buf;
        }
        cout << "\n";
    }

    // Vorab-Prüfung: Fenster jetzt schon vorhanden?
    if (!focusWindow.empty() && !findWindowByTitle(focusWindow)) {
        char buf[256];
        snprintf(buf, sizeof(buf), t(Str::WINDOW_NOT_FOUND_WARN),
                 toConsole(toWideArgv(focusWindow)).c_str());
        cout << buf << "\n" << flush;
    }

    // Schleife Direktanzeige von Zeit und Datum
    if (showLiveTime) {
        system("cls"); // Konsole bereinigen, damit nur die Zeit da steht.
        while (true) {
            auto now = chrono::system_clock::now();
            time_t tnow = chrono::system_clock::to_time_t(now);
            tm local{};
            localtime_s(&local, &tnow);

            // cout << "\r" << put_time(&local, "%Y-%m-%d %H:%M:%S") << flush;
            char timebuf[32];
            strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &local);
            cout << "\r" << timebuf << flush;
            char titlebuf[48];
            snprintf(titlebuf, sizeof(titlebuf), "Teefax - %s", timebuf);
            SetConsoleTitleA(titlebuf);

            // Berechne Millisekunden bis zur nächsten Sekunde
            auto next = chrono::time_point_cast<chrono::seconds>(now) + chrono::seconds(1);
            this_thread::sleep_until(next);
        }
        return 0; // Programm wird über Strg+C beendet
    }

    // Stoppuhr-Modus
    if (showStopwatch) {
        char buf[256];
        snprintf(buf, sizeof(buf), t(Str::STARTED), PRG_VERSION);
        cout << buf << " (" << t(Str::STOPWATCH_LABEL) << ")\n";
        cout << t(Str::STOPWATCH_HINT) << "\n";

        auto start   = chrono::steady_clock::now();
        bool isPaused = false;
        chrono::steady_clock::time_point pauseStart;
        long long frozenMs     = 0;  // Anzeige einfrieren bei Pause
        long long lastTitleSec = -1; // Fenstertitel nur einmal pro Sekunde

        while (true) {
            // Tasteneingabe prüfen (nicht-blockierend)
            if (_kbhit()) {
                int key = _getch();
                if (key == ' ' || key == 'p' || key == 'P') {
                    if (!isPaused) {
                        isPaused  = true;
                        pauseStart = chrono::steady_clock::now();
                        frozenMs  = chrono::duration_cast<chrono::milliseconds>(
                                       pauseStart - start).count();
                    } else {
                        // Startpunkt um Pausendauer verschieben, Elapsed bleibt korrekt
                        start    += chrono::steady_clock::now() - pauseStart;
                        isPaused  = false;
                    }
                } else if (key == 'r' || key == 'R') {
                    auto now     = chrono::steady_clock::now();
                    start        = now;
                    pauseStart   = now;
                    isPaused     = true;
                    frozenMs     = 0;
                    lastTitleSec = -1;
                }
            }

            long long elapsedMs = isPaused
                                      ? frozenMs
                                      : chrono::duration_cast<chrono::milliseconds>(
                                            chrono::steady_clock::now() - start).count();

            long long elapsedSec = elapsedMs / 1000;
            int       cs         = static_cast<int>((elapsedMs % 1000) / 10);

            string secStr = formatVerbleibend(elapsedSec);

            // Fenstertitel einmal pro Sekunde aktualisieren
            if (!isPaused && elapsedSec != lastTitleSec) {
                lastTitleSec = elapsedSec;
                wstring labelW = toWide(t(Str::STOPWATCH_LABEL));
                wstring titleW = L"Teefax - " + labelW + L" - "
                                 + wstring(secStr.begin(), secStr.end());
                SetConsoleTitleW(titleW.c_str());
            }

            char timebuf[64];
            snprintf(timebuf, sizeof(timebuf), "%s.%02d", secStr.c_str(), cs);

            char dispbuf[128];
            snprintf(dispbuf, sizeof(dispbuf), t(Str::ELAPSED), timebuf);

            // Pausenanzeige anhängen bzw. wegwischen (feste Breite durch Leerzeichen)
            char linebuf[256];
            if (isPaused)
                snprintf(linebuf, sizeof(linebuf), "%s %s", dispbuf, t(Str::STOPWATCH_PAUSED));
            else
                snprintf(linebuf, sizeof(linebuf), "%s", dispbuf);
            cout << "\r" << linebuf << "            " << flush;

            if (isPaused) {
                this_thread::sleep_for(chrono::milliseconds(10));
            } else {
                // Nächste 10ms-Grenze relativ zum Startpunkt. Echte Zentisekunden, ohne Drift
                auto nextTick = start
                                + chrono::milliseconds(((elapsedMs / 10) + 1) * 10);
                this_thread::sleep_until(nextTick);
            }
        }
        return 0; // Programm wird über Strg+C beendet
    }


    // Die normale Timer-Schleife
    do {
        if (loop && loopCount < std::numeric_limits<long long>::max()) ++loopCount;

        // Für --daily und --at: absoluten Wanduhr-Zielzeitpunkt frisch bestimmen
        chrono::system_clock::time_point wallTarget;
        if (useDailyTimes) {
            wallTarget = nextDailyTarget(dailyTimes);
        } else if (useEvery) {
            wallTarget = nextEveryTarget(everySpec);
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
            // Ziel als absoluter system_clock-Zeitpunkt, NTP-Korrekturen werden automatisch berücksichtigt.
            wallTarget = chrono::system_clock::now() + chrono::milliseconds(ms);
        }

        long long totalMsThisRound = (useDailyTimes || useEvery || useAtTime)
                                         ? chrono::duration_cast<chrono::milliseconds>(
                                               wallTarget - chrono::system_clock::now()).count()
                                         : ms;

        // Wanduhr-Ziel als time_t für Tomorrow-Anzeige
        time_t wallTargetT = 0;
        if (useDailyTimes || useEvery || useAtTime) {
            wallTargetT = chrono::system_clock::to_time_t(wallTarget);
        }

        auto start = chrono::steady_clock::now();
        auto end   = start + chrono::milliseconds(totalMsThisRound);

        // customMsg einmalig in wstring konvertieren. Aendert sich eh nie waehrend des Durchlaufs.
        wstring customMsgW;
        if (!customMsg.empty()) {
            customMsgW = toWideArgv(customMsg);
            const size_t maxLen = 30;
            if (customMsgW.size() > maxLen)
                customMsgW = customMsgW.substr(0, maxLen) + L"...";
        }

        long long lastVerbleibendSec = -1;
        bool soundPrewarmed  = false; // Bluetooth-Prewarm: einmalig pro Durchlauf
        bool preAlarmStarted = false; // Voralarm-WAV: einmalig pro Durchlauf gestartet
        vector<uint8_t> preAlarmWavBuf; // Haelt den Voralarm-Puffer am Leben (SND_MEMORY | SND_ASYNC)

        while (true) {
            auto nowSteady = chrono::steady_clock::now();
            auto nowWall   = chrono::system_clock::now();

            bool done = false;
            long long verbleibendMs = 0;

            if (useDailyTimes || useEvery || useAtTime) {
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

                // BT-PREWARM (nur ohne Voralarm): Stille-Loop 2 s vor dem Alarm starten.
                // Bei aktivem Voralarm entfaellt das. Die Voralarm-WAV unten
                // enthaelt bereits Stille am Anfang und haelt BT durchgehend warm.
                if (!mute && !asyncSound && !soundPrewarmed && preAlarmSeconds == 0 &&
                    verbleibendSec > 0 && verbleibendSec <= 2)
                {
                    soundPrewarmed = true;
                    PlaySoundA(reinterpret_cast<LPCSTR>(silentWav().data()),
                               NULL, SND_MEMORY | SND_ASYNC | SND_LOOP);
                }

                // VORALARM: Einmalig pro Durchlauf einen WAV-Puffer aufbauen und abspielen.
                // Der Puffer enthaelt Stille (BT-Prewarm) gefolgt von allen Beeps.
                // Ein kontinuierlicher Audio-Stream ohne Unterbrechung.
                // Kein SND_LOOP: der Puffer endet nach dem letzten Beep von selbst.
                // Der Alarm-PlaySoundA am Durchlaufende unterbricht automatisch.
                // preAlarmWavBuf haelt den Puffer am Leben bis der Stream endet.
                if (preAlarmSeconds > 0 && !preAlarmStarted &&
                    verbleibendSec > 0 && verbleibendSec <= static_cast<long long>(preAlarmSeconds) + 2)
                {
                    preAlarmStarted = true;
                    // Anzahl verbleibender Beeps: kuerzer als preAlarmSeconds wenn der Timer
                    // insgesamt kuerzer als preAlarmSeconds + 2 s ist.
                    int beepCount  = static_cast<int>(
                        min(verbleibendSec, static_cast<long long>(preAlarmSeconds)));
                    // Stille-Prewarm: bis zu 2000 ms, je nachdem wie viel Zeit bleibt.
                    int prewarmMs  = static_cast<int>(
                        min(2000LL, (verbleibendSec - static_cast<long long>(beepCount)) * 1000LL));
                    if (prewarmMs < 0) prewarmMs = 0;

                    preAlarmWavBuf = buildPreAlarmWav(beepCount, prewarmMs);
                    if (!preAlarmWavBuf.empty()) {
                        PlaySoundA(reinterpret_cast<LPCSTR>(preAlarmWavBuf.data()),
                                   NULL, SND_MEMORY | SND_ASYNC);
                    }
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

                {
                    // verbleibendStr enthaelt nur ASCII (Ziffern + y/mo/d/h/m/s)
                    wstring titleW = L"Teefax - " + wstring(verbleibendStr.begin(), verbleibendStr.end());
                    if (!customMsgW.empty())
                        titleW += L" | " + customMsgW;
                    SetConsoleTitleW(titleW.c_str()); // Zeit im Fenstertitel anzeigen
                }

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
                cout << "]        " << flush;
            }

            // Bis zur nächsten Sekundengrenze schlafen.
            // Wandzeitmodi (--at, --daily, --every):
            //   Zielpunkt wird aus wallTarget (system_clock) berechnet,
            //   der eigentliche Schlaf läuft aber auf steady_clock.
            //   So bleiben NTP-Vorwärtskorrekturen wirksam, während
            //   Rückwärtssprünge den Thread nicht einfrieren.
            //   Obergrenze 1,5s: nach einem Rückwärtssprung wird spätestens
            //   dann neu ausgewertet, damit die Anzeige nicht einfriert.
            // Countdown-Modus (5m, 1h30m ...):
            //   sleep_until auf steady_clock direkt, also kein Drift möglich.
            if (verbleibendSec >= 1) {
                if (useDailyTimes || useEvery || useAtTime) {
                    auto nextWallTick   = wallTarget - chrono::seconds(verbleibendSec - 1);
                    auto durationToTick = nextWallTick - chrono::system_clock::now();
                    if (durationToTick > chrono::milliseconds(1500))
                        durationToTick = chrono::milliseconds(1500);
                    if (durationToTick > chrono::milliseconds(0))
                        this_thread::sleep_until(
                            chrono::steady_clock::now() +
                            chrono::duration_cast<chrono::milliseconds>(durationToTick));
                } else {
                    this_thread::sleep_until(
                        end - chrono::seconds(verbleibendSec - 1));
                }
            }
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
        cout << "]        " << flush;

        // \n nach dem vollen Balken:
        // - Immer beim letzten Durchlauf (TIMER_ENDED, MessageBox folgen).
        // - Immer wenn --cmd aktiv ist: CMD_STARTED und Prozessausgabe sollen
        //   auf eigenen Zeilen stehen, nicht neben dem Balken.
        // - --open und --focus: kein eigenes \n noetig (kein Konsolentext bei Erfolg;
        //   Fehler steuern \n selbst via \r/printErr bzw. Abbruchmeldung).
        bool isLastIteration = !loop || (maxLoops != -1 && loopCount >= maxLoops);
        bool hasPostOutput   = !cmdArg.empty();
        if (isLastIteration || hasPostOutput)
            cout << "\n" << flush;

        if (!mute) {
            for (long long r = 0; alarmRepeat == 0 || r < alarmRepeat; ++r) {
                if (!soundFile.empty()) {
                    try {
                        fs::path p(soundFile);
                        if (!fs::exists(p) || !fs::is_regular_file(p)) {
                            char buf[512];
                            snprintf(buf, sizeof(buf), t(Str::AUDIO_NOT_FOUND),
                                     toConsole(toWideArgv(soundFile)).c_str());
                            cout << buf << "\n";
                        } else {
                            wstring widePath = toWideArgv(soundFile);
                            if (!widePath.empty()) {
                                UINT flags = SND_FILENAME | (asyncSound ? SND_ASYNC : SND_SYNC);
                                PlaySoundW(widePath.c_str(), NULL, flags);
                            } else {
                                char buf[512];
                                snprintf(buf, sizeof(buf), t(Str::AUDIO_PATH_ERROR),
                                         toConsole(toWideArgv(soundFile)).c_str());
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
                if (alarmRepeat == 0 || r < alarmRepeat - 1) this_thread::sleep_for(chrono::seconds(alarmInterval));
            }
        }

        // Nächste Wartezeit berechnen (nur noch für --daily / --every nötig, --at läuft oben)
        if (useDailyTimes) {
            wallTarget = nextDailyTarget(dailyTimes);
            ms = chrono::duration_cast<chrono::milliseconds>(
                     wallTarget - chrono::system_clock::now()).count();
            if (ms <= 0) ms = 1000;
        } else if (useEvery) {
            wallTarget = nextEveryTarget(everySpec);
            ms = chrono::duration_cast<chrono::milliseconds>(
                     wallTarget - chrono::system_clock::now()).count();
            if (ms <= 0) ms = 1000;
        }

        // Konsolenmodus wiederherstellen, damit Kind-Prozesse (etwa via --cmd gestartet)
        // die originale Einstellung erben und sie beim Beenden korrekt zurücksetzen können.
        if (g_consoleModeChanged) {
            SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), g_originalConsoleMode);
            g_consoleModeChanged = false;
        }

        // Datei öffnen oder Konsolenbefehl ausführen
        if (!cmdArg.empty()) {
            runConsoleCommand(cmdArg);
        }

        // QuickEdit nach jedem Durchlauf neu deaktivieren.
        // Bei --cmd: das Kind hat ggf. den Originalzustand (QuickEdit an) beim Beenden
        // wiederhergestellt. Bei --daily / --every ohne --cmd: die Wiederherstellung oben
        // schaltet QuickEdit ebenfalls zurück. In beiden Fällen muss es hier erneut
        // abgeschaltet werden, damit der Timer in Schleifen durchgehend QuickEdit-frei bleibt.
        if (loop) {
            HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
            DWORD mode = 0;
            if (GetConsoleMode(hIn, &mode)) {
                mode &= ~ENABLE_QUICK_EDIT_MODE;
                mode |=  ENABLE_EXTENDED_FLAGS;
                SetConsoleMode(hIn, mode);
                g_consoleModeChanged = true;
            }
        }
        if (!openFile.empty()) {
            // Wie --focus: bei Fehler (Datei nicht gefunden) Schleife abbrechen.
            if (!openFileAfterTimer(openFile) && loop) {
                loop = false;
            }
        }
        if (!focusWindow.empty()) {
            if (!bringWindowToFront(focusWindow)) {
                char buf[256];
                snprintf(buf, sizeof(buf), t(Str::WINDOW_NOT_FOUND_ABORT),
                         toConsole(toWideArgv(focusWindow)).c_str());
                cout << buf;
                loop = false;
            }
        }

        // Fenstertitel zurücksetzen, bevor die Benachrichtigung den Fokus/Thread blockiert.
        SetConsoleTitleA("Teefax");

        // Benachrichtigung, Zeit abgelaufen
        if (showMessage) {
            wstring notifyText = toWide(t(Str::NOTIFY_MSG));
            if (!customMsg.empty())
                notifyText += L"\n\n" + toWideArgv(customMsg); // benutzerdefinierte Notiz anhängen
            showNotification(
                toWide(t(Str::NOTIFY_TITLE)),
                notifyText
                );
        }

    } while (loop && (maxLoops == -1 || loopCount < maxLoops));
    cout << t(Str::TIMER_ENDED);

    // Bildschirmschoner wieder erlauben
    if (noSleep){
        preventSleep(false);
    }

    // Konsolenmodus wiederherstellen
    if (g_consoleModeChanged) {
        SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), g_originalConsoleMode);
        g_consoleModeChanged = false;
    }

    return 0;
}