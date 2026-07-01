#include <iostream>
#include "i18n.h" // Übersetzungen
#include <chrono>
#include <thread>
#include <string>
#include <cmath>
#include <ctime>
#include <windows.h>
#include <mmsystem.h>

// SDK-Kompatibilitaet: PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION
// erst ab Windows SDK 10.0.22000 (Windows 11) definiert.
// Numerischer Wert laut Microsoft-Dokumentation unveraendert.
#ifndef PROCESS_POWER_THROTTLING_CURRENT_VERSION
#define PROCESS_POWER_THROTTLING_CURRENT_VERSION 1
#endif
#ifndef PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION
#define PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION 0x4
#endif
#include <filesystem>
#include <limits>
#include <cctype>
#include <atomic>
#include "sound_array.h" // Signalton
#include <vector> // Für täglichen Alarm
#include <sstream> // Für --every Parsing/Formatierung
#include <conio.h> // _kbhit(), _getch() für Stoppuhr-Pause
#include <algorithm>
#include <optional>



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
BOOL WINAPI ConsoleHandler(DWORD /*signal*/) {
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
static bool g_timePeriodOk = false;

struct TimePeriodGuard {
    TimePeriodGuard() {
        if (timeBeginPeriod(1) == TIMERR_NOERROR) {
            g_timePeriodSet.store(true);
            g_timePeriodOk = true;
        }
    }
    ~TimePeriodGuard() {
        if (g_timePeriodSet.load()) {
            timeEndPeriod(1);
            g_timePeriodSet.store(false);
        }
    }
};

// RAII-Hilfe: hebt die Thread-Priorität auf TIME_CRITICAL für die Dauer des Timer-Laufs.
// Verringert den Aufwachversatz von ~1-3 ms auf ~0,5-1 ms, da der Thread beim Weckruf
// des Schedulers bevorzugt sofort ausgeführt wird. Kein CPU-Mehrverbrauch, weil der
// Thread die überwiegende Zeit in sleep_until schläft.
struct PriorityGuard {
    int oldPriority;
    PriorityGuard() {
        oldPriority = GetThreadPriority(GetCurrentThread());
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    }
    ~PriorityGuard() {
        SetThreadPriority(GetCurrentThread(), oldPriority);
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

long long safeStoll(const string& s, long long fallback = 0) {
    try {
        size_t pos = 0;
        long long v = stoll(s, &pos, 10);
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
        cout << buf << "\n";
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

// ── Julianische Tageszahl und mktime-freie Zeitdifferenz ─────────────

// Gregorianisches Datum → Julianische Tageszahl nach Richards (2013).
// Gültig für alle positiven Jahre im proleptischen gregorianischen Kalender,
// weit jenseits des mktime-Bereichs auf Windows (~Jahr 3001).
static long long toJulianDayNumber(int year, int month, int day) {
    long long a = (14LL - month) / 12LL;
    long long y = (long long)year + 4800LL - a;
    long long m = (long long)month + 12LL * a - 3LL;
    return (long long)day
           + (153LL * m + 2LL) / 5LL
           + 365LL * y
           + y / 4LL
           - y / 100LL
           + y / 400LL
           - 32045LL;
}

// Millisekunden bis zu einem weit in der Zukunft liegenden Datum.
// Fallback für millisecondsUntilDateTime(), wenn mktime() scheitert.
// Ignoriert Sommerzeitübergänge am Zielzeitpunkt (bei Jahrtausenden irrelevant).
static long long millisecondsUntilDateTimeFar(int year, int month, int day,
                                              int hour, int minute, int second)
{
    using namespace chrono;
    auto   now   = system_clock::now();
    time_t tnow  = system_clock::to_time_t(now);
    tm     local{};
    if (localtime_s(&local, &tnow) != 0) return 0;

    long long jdnNow    = toJulianDayNumber(local.tm_year + 1900,
                                         local.tm_mon  + 1,
                                         local.tm_mday);
    long long jdnTarget = toJulianDayNumber(year, month, day);
    long long dayDiff   = jdnTarget - jdnNow;

    long long secNow    = (long long)local.tm_hour * 3600LL
                       + (long long)local.tm_min  * 60LL
                       + (long long)local.tm_sec;
    long long secTarget = (long long)hour   * 3600LL
                          + (long long)minute * 60LL
                          + (long long)second;

    long long totalSec = dayDiff * 86400LL + secTarget - secNow;

    // Sub-Sekunden-Anteil der aktuellen Zeit abziehen
    long long msNow   = duration_cast<milliseconds>(
                          now.time_since_epoch()).count() % 1000LL;
    long long totalMs = totalSec * 1000LL - msNow;

    if (totalMs <= 0) return 0;
    return clampMs(static_cast<long double>(totalMs));
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

    // mktime scheitert auf Windows UCRT für Jahre > ~3000 (gibt -1 zurück).
    // Fallback auf JDN-basierte Berechnung ohne Systemfunktionen.
    if (target_t == -1)
        return millisecondsUntilDateTimeFar(year, month, day, hour, minute, second);

    // system_clock::from_time_t() wird bewusst NICHT verwendet: MinGW nutzt intern
    // Nanosekunden (int64_t), womit der Wertebereich nur bis ~Jahr 2262 reicht.
    // Für Jahre ab 2262 überläuft from_time_t() still und liefert einen falschen
    // time_point. Stattdessen: time_t-Differenz in Sekunden, dann ms umrechnen.
    time_t tnow = system_clock::to_time_t(now);
    if (target_t <= tnow)
        return 0; // Zeitpunkt liegt in der Vergangenheit

    long long diffSec = static_cast<long long>(target_t)
                        - static_cast<long long>(tnow);

    // Sub-Sekunden-Anteil abziehen (epochen-unabhängig)
    long long msNow   = duration_cast<milliseconds>(
                          now.time_since_epoch()).count() % 1000LL;
    long long totalMs = diffSec * 1000LL - msNow;
    if (totalMs <= 0) return 0;
    return clampMs(static_cast<long double>(totalMs));
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

    // QuickEdit sofort nach Prozessstart im Elternprozess neu deaktivieren.
    // Das Kind hat den originalen Konsolenmodus bereits geerbt; der Elternprozess
    // braucht QuickEdit nicht: ein Mausklick waehrend WaitForSingleObject wuerde
    // den Timer einfrieren. Das nachgelagerte if(loop)-Abschnitt in main() bleibt
    // weiterhin zustaendig fuer den Fall, dass das Kind den Modus beim Beenden
    // wiederhergestellt hat (z. B. verschachteltes teefax).
    // g_consoleModeChanged wird auf true gesetzt, damit der ConsoleHandler bei
    // Strg+C den Originalmodus (QuickEdit an) korrekt wiederherstellt.
    {
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        DWORD  mode = 0;
        if (GetConsoleMode(hIn, &mode)) {
            mode &= ~ENABLE_QUICK_EDIT_MODE;
            mode |=  ENABLE_EXTENDED_FLAGS;
            SetConsoleMode(hIn, mode);
            g_consoleModeChanged = true;
        }
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

// Verhindert, dass Windows 10/11 auf Akkubetrieb timeBeginPeriod(1) stillschweigend
// zurückdreht (Power Throttling). Kein RAM-, kein CPU-Mehrverbrauch: der Kernel
// setzt lediglich ein Flag im Prozesskontrollblock.
// Nur im Normalmodus aufrufen; im --eco-Modus ist timeBeginPeriod ohnehin deaktiviert.
static void applyPowerThrottlingExemption() {
    PROCESS_POWER_THROTTLING_STATE pts = {};
    pts.Version     = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    pts.ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
    pts.StateMask   = 0; // 0 = Drosselung abschalten
    SetProcessInformation(GetCurrentProcess(),
                          ProcessPowerThrottling, &pts, sizeof(pts));
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
    // localtime_s scheitert auf Windows für time_t-Werte jenseits ~Jahr 3001.
    // Für solche Daten ist der "morgen"-Hinweis ohnehin nicht anwendbar.
    constexpr time_t SAFE_MAX_T = 32503680000LL; // ~Jahr 3001
    if (targetT <= 0 || targetT > SAFE_MAX_T) return false;

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
    bool typeSet = false;
    while (getline(ss, token, ',')) {
        if (token.empty()) continue;
        int wd = parseWeekday(token);
        if (wd >= 0) {
            if (typeSet && spec.type != EverySpec::Type::Weekday) {
                spec.days.clear(); // gemischte Typen → Fehler
                return spec;
            }
            spec.type = EverySpec::Type::Weekday;
            typeSet = true;
            spec.days.push_back(wd);
        } else {
            int day = safeStoi(token, -1);
            if (day >= 1 && day <= 31) {
                if (typeSet && spec.type != EverySpec::Type::MonthDay) {
                    spec.days.clear(); // gemischte Typen → Fehler
                    return spec;
                }
                spec.type = EverySpec::Type::MonthDay;
                typeSet = true;
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
    DWORD len = GetModuleFileNameW(nullptr, exeBuf, MAX_PATH);
    if (len == 0) return;
    if (len >= MAX_PATH) {
        char w[256]; snprintf(w, sizeof(w), t(Str::WARN_PATH_TOO_LONG), MAX_PATH); fprintf(stderr, "%s\n", w);
        return;
    }

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
    DWORD len = GetModuleFileNameW(nullptr, exeBuf, MAX_PATH);
    if (len == 0) return L"";
    if (len >= MAX_PATH) {
        char w[256]; snprintf(w, sizeof(w), t(Str::WARN_PATH_TOO_LONG), MAX_PATH); fprintf(stderr, "%s\n", w);
        return L"";
    }
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
        "--macro","--for"
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

static bool isMacroSubCmdValid(const string& subcmd) {
    return subcmd == "add" || subcmd == "remove" || subcmd == "list";
    // if (subcmd.empty()) return false;
    // if (subcmd != "add" && subcmd != "remove" && subcmd != "list") return false;
    // return true;
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

// Erzeugt einen ~10 ms langen Stille-Puffer (einmalig).
// Zweck: winmm synchron initialisieren, bevor der Voralarm-Puffer abgespielt wird.
// Ohne diesen Aufruf kann die Audio-Initialisierung (100-500 ms auf Frischstarts)
// in die Voralarm-Stille fallen und den ersten Beep verschlucken.
static const vector<uint8_t>& tinyInitWav() {
    static vector<uint8_t> wav = [](){
        constexpr uint32_t RATE  = 22050;
        constexpr uint32_t DSIZE = (RATE / 100) * 2; // ~10 ms, 16-bit mono = 440 Bytes
        vector<uint8_t> w(44 + DSIZE, 0);
        auto w16 = [&](size_t o, uint16_t v){ memcpy(w.data()+o, &v, 2); };
        auto w32 = [&](size_t o, uint32_t v){ memcpy(w.data()+o, &v, 4); };
        memcpy(w.data()+0,  "RIFF", 4); w32(4,  36 + DSIZE);
        memcpy(w.data()+8,  "WAVE", 4);
        memcpy(w.data()+12, "fmt ", 4); w32(16, 16);
        w16(20, 1); w16(22, 1); w32(24, RATE); w32(28, RATE*2); w16(32, 2); w16(34, 16);
        memcpy(w.data()+36, "data", 4); w32(40, DSIZE);
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
    // Amplitude des Voralarm-Beeps: reiner Sinuston klingt psychoakustisch lauter
    // als ein gleich starkes Breitbandsignal; daher deutlich unter dem Pegel des
    // eingebetteten Alarmtons halten. Bereich 0.15..0.40 je nach Geschmack.
    constexpr float    AMPLITUDE    = 0.25f;

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
            float s = env * AMPLITUDE * 32767.0f * sinf(PI2 * FREQ * static_cast<float>(i) / static_cast<float>(RATE)); // Amplitude oben ändern
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


// ═══════════════════════════════════════════════════════════════════════════
// ── Konfigurationsstruktur ─────────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════════════════

// Fasst alle geparsten Einstellungen zusammen.
// Felder mit Laufzeit-Zustand (loopCount, atYear) werden zur Laufzeit verändert.
struct TimerConfig {
    // Ton
    string    soundFile;
    bool      mute          = false;
    bool      asyncSound    = false;
    long long alarmRepeat   = 1;
    int       alarmInterval = 2;

    // Zeitangabe
    long long ms            = 0;
    bool      useAtTime     = false;
    bool      useAtDateTime = false;
    int       atYear = 0, atMonth = 0, atDay = 0;
    int       atHour = 0, atMinute = 0, atSecond = 0;

    // Schleife
    bool      loop     = false;
    int       maxLoops = -1;

    // Voralarm
    int       preAlarmSeconds = 0;

    // Benachrichtigung
    bool      showMessage = true;
    string    customMsg;

    // Aktionen nach Ablauf
    string    openFile;
    string    cmdArg;
    string    focusWindow;

    // Modi
    bool      showLiveTime  = false;
    bool      showStopwatch = false;
    bool      noSleep       = false;
    bool      eco           = false;

    // Täglicher / wiederkehrender Alarm
    vector<tuple<int,int,int>> dailyTimes;
    bool      useDailyTimes = false;
    EverySpec everySpec;
    bool      useEvery      = false;

    // --for: Gesamtlaufzeit der Schleife begrenzen
    long long forMs  = 0;     // 0 = deaktiviert
    bool      useFor = false;

    // Schleifenzustand – wird zur Laufzeit verändert
    long long loopCount = 0;
};

// ═══════════════════════════════════════════════════════════════════════════
// ── Konsolenmodus-Helfer ───────────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════════════════

// Deaktiviert QuickEdit im aktuellen Konsolenfenster.
// Verhindert, dass ein Mausklick den Timer einfriert.
static void disableQuickEdit() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD  mode = 0;
    if (GetConsoleMode(hIn, &mode)) {
        mode &= ~ENABLE_QUICK_EDIT_MODE;
        mode |=  ENABLE_EXTENDED_FLAGS;
        SetConsoleMode(hIn, mode);
        g_consoleModeChanged = true;
    }
}

// Stellt den gespeicherten Originalzustand des Konsolenmodus wieder her.
static void restoreConsoleMode() {
    if (g_consoleModeChanged) {
        SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), g_originalConsoleMode);
        g_consoleModeChanged = false;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// ── Argument-Vorverarbeitung ───────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════════════════

// Setzt TEEFAX_LANG anhand des letzten --lang-Vorkommens in der Argumentliste.
// Wird zweimal aufgerufen: vor dem --macro-Handler (damit dessen Meldungen
// bereits in der gewählten Sprache erscheinen) und nach der Makro-Expansion
// (damit CLI-Argumente einen INI-Wert überschreiben können).
static void detectLanguageFromArgs(const vector<string>& args) {
    for (int i = 0; i + 1 < static_cast<int>(args.size()); ++i) {
        if (args[i] == "--lang" || args[i] == "-la")
            _putenv_s("TEEFAX_LANG", args[i + 1].c_str());
    }
}

// Wertet --macro-Unterbefehle aus und kehrt sofort zurück.
// Rückgabe: 0 oder 1 (Exitcode) wenn --macro gefunden wurde; -1 wenn nicht.
static int handleMacroCommands(vector<string>& args) {
    const int n = static_cast<int>(args.size());
    for (int i = 0; i < n; ++i) {
        if (args[i] != "--macro") continue;

        if (i + 1 >= n) {
            cout << t(Str::MACRO_MISSING_SUBCMD) << "\n";
            return 1;
        }

        string subcmd = args[i + 1];
        char   buf[256];

        if (!isMacroSubCmdValid(subcmd)) {
            snprintf(buf, sizeof(buf), t(Str::MACRO_INVALID_SUBCMD), subcmd.c_str());
            cout << buf << "\n";
            return 1;
        }

        // ── list ──────────────────────────────────────────────────────
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

        // ── remove ────────────────────────────────────────────────────
        if (subcmd == "remove") {
            if (i + 2 >= n) { cout << t(Str::MACRO_MISSING_NAME) << "\n"; return 1; }
            string name = args[i + 2];
            if (removeMacroFromIni(name)) {
                snprintf(buf, sizeof(buf), t(Str::MACRO_REMOVED), name.c_str());
                cout << buf << "\n";
                return 0;
            } else {
                snprintf(buf, sizeof(buf), t(Str::MACRO_NOT_FOUND), name.c_str());
                cout << buf << "\n";
                return 1;
            }
        }

        // ── add ───────────────────────────────────────────────────────
        if (subcmd == "add") {
            if (i + 2 >= n) { cout << t(Str::MACRO_MISSING_NAME) << "\n"; return 1; }
            string name = args[i + 2];

            if (!isMacroNameValid(name)) {
                snprintf(buf, sizeof(buf), t(Str::MACRO_INVALID_NAME), name.c_str());
                cout << buf << "\n"; return 1;
            }
            if (isMacroNameReserved(name)) {
                snprintf(buf, sizeof(buf), t(Str::MACRO_NAME_RESERVED), name.c_str());
                cout << buf << "\n"; return 1;
            }
            if (i + 3 >= n) { cout << t(Str::MACRO_MISSING_ARGS) << "\n"; return 1; }

            // Alle verbleibenden Token als Makro-Body zusammenfügen.
            // Quoting-Regeln: Token mit Leerzeichen oder nach einem Wert-Flag -> quoten.
            static const vector<string> valueFlags = {
                "--focus", "-f", "--msg", "--open", "-o", "--cmd", "-c",
                "--at", "-a", "--until", "--lang", "-la",
                "--alarm-repeat", "-ar", "--alarm-interval", "-ai",
                "--prealarm", "-pa", "--loop", "-l",
                "--every", "-e", "--daily", "-d",
            };
            string macroArgs;
            bool nextNeedsQuotes = false;
            for (int j = i + 3; j < n; ++j) {
                if (j > i + 3) macroArgs += " ";
                const string& tok = args[j];
                bool needsQuotes = nextNeedsQuotes || tok.find(' ') != string::npos;
                nextNeedsQuotes = false;
                for (const auto& vf : valueFlags)
                    if (tok == vf) { nextNeedsQuotes = true; break; }
                if (needsQuotes) macroArgs += '"';
                macroArgs += tok;
                if (needsQuotes) macroArgs += '"';
            }

            // Rückfrage wenn Makro bereits existiert
            auto macros = loadMacros();
            if (macros.count(name)) {
                snprintf(buf, sizeof(buf), t(Str::MACRO_OVERWRITE_PROMPT), name.c_str());
                cout << buf << flush;
                if (g_consoleModeChanged)
                    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), g_originalConsoleMode);
                string answer;
                getline(cin, answer);
                if (g_consoleModeChanged) {
                    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
                    DWORD  mode = 0;
                    if (GetConsoleMode(hIn, &mode)) {
                        mode &= ~ENABLE_QUICK_EDIT_MODE;
                        mode |=  ENABLE_EXTENDED_FLAGS;
                        SetConsoleMode(hIn, mode);
                    }
                }
                string yesChars = t(Str::MACRO_OVERWRITE_YES);
                if (answer.empty() || yesChars.find(answer[0]) == string::npos)
                    return 0;
            }
            saveMacroToIni(name, macroArgs);
            snprintf(buf, sizeof(buf), t(Str::MACRO_ADDED), name.c_str());
            cout << buf << "\n";
            return 0;
        }
    }
    return -1; // kein --macro gefunden
}

// Ersetzt den ersten passenden CLI-Makronamen durch seine gespeicherten Tokens.
// Nur CLI-Argumente werden geprüft (ab cliStart), damit INI-Tokens nicht
// versehentlich als Makronamen ausgewertet werden.
static void expandMacroInArgs(vector<string>& args, int argc) {
    auto macros = loadMacros();
    int cliStart = static_cast<int>(args.size()) - (argc - 1);
    if (cliStart < 0) cliStart = 0;

    // Flags, deren naechstes Token ein freier Wert ist (kein Makroname)
    static const vector<string> valueFlags = {
        "--msg",           "--cmd",    "-c",       "--open",   "-o",
        "--focus",         "-f",       "--at",     "-a",   "--until",
        "--lang",          "-la",      "--for",
        "--alarm-repeat",  "-ar",      "--alarm-interval", "-ai",
        "--prealarm",      "-pa",      "--loop",   "-l",
        "--every",         "-e",       "--daily",  "-d"
    };

    bool skipNext = false;
    for (int i = cliStart; i < static_cast<int>(args.size()); ++i) {
        const string& a = args[i];
        if (skipNext) { skipNext = false; continue; }
        if (a[0] == '-') {
            for (const auto& vf : valueFlags)
                if (a == vf) { skipNext = true; break; }
            continue;
        }
        auto it = macros.find(a);
        if (it != macros.end()) {
            vector<string> expanded = tokenizeConfigLine(it->second);
            args.erase(args.begin() + i);
            args.insert(args.begin() + i, expanded.begin(), expanded.end());
            break; // nur einmal expandieren
        }
    }
}

// Deaktiviert QuickEdit, sofern ein Timer gestartet wird.
// Ausgenommen: --help, --version und Aufrufe ohne Argumente.
static void setupQuickEdit(const vector<string>& args, int argc) {
    if (argc < 2) return;
    for (const auto& a : args)
        if (a == "--help" || a == "-h" || a == "--version" || a == "-v") return;

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD  mode = 0;
    if (GetConsoleMode(hIn, &mode)) {
        g_originalConsoleMode = mode;
        g_consoleModeChanged  = true;
        mode &= ~ENABLE_QUICK_EDIT_MODE;
        mode |=  ENABLE_EXTENDED_FLAGS;
        SetConsoleMode(hIn, mode);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// ── Argument-Parser ────────────────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════════════════

// Parst die kombinierte Argumentliste (INI + CLI) in cfg.
// Rückgabe: -1 = Timer starten; 0 = Exit OK; 1 = Exit Fehler.
static int parseArguments(const vector<string>& args, TimerConfig& cfg) {
    const int nArgs = static_cast<int>(args.size());

    for (int i = 0; i < nArgs; ++i) {
        const string& arg = args[i];

        if (arg == "--nosleep" || arg == "-ns") {
            cfg.noSleep = true;

        } else if (arg == "--eco") {
            cfg.eco = true;

        } else if (arg == "--mute" || arg == "-m") {
            cfg.mute = true;

        } else if (arg == "--loop" || arg == "-l") {
            cfg.loop = true;
            if (i + 1 < nArgs) {
                int possibleCount = safeStoi(args[i + 1], -1);
                if (possibleCount > 0) { cfg.maxLoops = possibleCount; ++i; }
            }

        } else if (arg == "--nomsg") {
            cfg.showMessage = false;

        } else if (arg == "--msg" && i + 1 < nArgs) {
            cfg.customMsg = args[++i];

        } else if ((arg == "--alarm-repeat" || arg == "-ar") && i + 1 < nArgs) {
            const string& val = args[++i];
            try {
                size_t pos = 0;
                cfg.alarmRepeat = stoll(val, &pos, 10);
                if (pos != val.size() || cfg.alarmRepeat < 0) {
                    cfg.alarmRepeat = 1;
                    char w[256]; snprintf(w, sizeof(w), t(Str::WARN_ALARM_REPEAT_INVALID), val.c_str()); fprintf(stderr, "%s\n", w);
                }
            } catch (const std::out_of_range&) {
                cfg.alarmRepeat = std::numeric_limits<long long>::max();
                char w[256]; snprintf(w, sizeof(w), t(Str::WARN_ALARM_REPEAT_TOO_LARGE), val.c_str(), cfg.alarmRepeat); fprintf(stderr, "%s\n", w);
            } catch (...) {
                cfg.alarmRepeat = 1;
                char w[256]; snprintf(w, sizeof(w), t(Str::WARN_ALARM_REPEAT_INVALID), val.c_str()); fprintf(stderr, "%s\n", w);
            }

        } else if ((arg == "--alarm-interval" || arg == "-ai") && i + 1 < nArgs) {
            cfg.alarmInterval = safeStoi(args[++i], 2);
            if (cfg.alarmInterval < 1) cfg.alarmInterval = 1;

        } else if (arg == "--async" || arg == "-as") {
            cfg.asyncSound = true;

        } else if ((arg == "--at" || arg == "-a" || arg == "--until") && i + 1 < nArgs) {
            string first = args[++i];
            int year, month, day;
            int hour = 0, minute = 0, second = 0;

            if (sscanf(first.c_str(), "%d-%d-%d", &year, &month, &day) == 3) {
                // Fall 1: Datum erkannt – optionale Uhrzeit prüfen
                if (i + 1 < nArgs && args[i + 1][0] != '-') {
                    int parsed = sscanf(args[i + 1].c_str(), "%d:%d:%d", &hour, &minute, &second);
                    if (parsed >= 2) { if (parsed == 2) second = 0; ++i; }
                    else { hour = minute = second = 0; }
                }
                cfg.atYear = year; cfg.atMonth = month; cfg.atDay = day;
                cfg.atHour = hour; cfg.atMinute = minute; cfg.atSecond = second;
                cfg.useAtTime = cfg.useAtDateTime = true;
                cfg.ms = millisecondsUntilDateTime(year, month, day, hour, minute, second);
                if (cfg.ms == 0) { cout << t(Str::ERROR_PAST_DATETIME) << "\n"; return 1; }
            } else {
                // Fall 2: Nur Uhrzeit
                int parsed = sscanf(first.c_str(), "%d:%d:%d", &cfg.atHour, &cfg.atMinute, &cfg.atSecond);
                if (parsed < 2) { cout << t(Str::ERROR_INVALID_AT) << "\n"; return 1; }
                if (parsed == 2) cfg.atSecond = 0;
                cfg.useAtTime = true;
                cfg.ms = millisecondsUntilTime(cfg.atHour, cfg.atMinute, cfg.atSecond);
                if (cfg.ms == 0) { cout << t(Str::ERROR_NEXT_TIME); return 1; }
            }

        } else if ((arg == "--open" || arg == "-o") && i + 1 < nArgs) {
            cfg.openFile = args[++i];

        } else if ((arg == "--cmd" || arg == "-c") && i + 1 < nArgs) {
            cfg.cmdArg = args[++i];

        } else if ((arg == "--focus" || arg == "-f") && i + 1 < nArgs) {
            cfg.focusWindow = args[++i];

        } else if ((arg == "--prealarm" || arg == "-pa") && i + 1 < nArgs) {
            cfg.preAlarmSeconds = safeStoi(args[++i], 0);
            if (cfg.preAlarmSeconds < 0) cfg.preAlarmSeconds = 0;

        } else if (arg == "--time" || arg == "-t") {
            cfg.showLiveTime = true;

        } else if (arg == "--stopwatch" || arg == "-sw") {
            cfg.showStopwatch = true;

        } else if (arg == "--daily" || arg == "-d") {
            cfg.useDailyTimes = true;
            while (i + 1 < nArgs && args[i + 1][0] != '-') {
                int h = 0, m = 0, s = 0;
                int parsed = sscanf(args[i + 1].c_str(), "%d:%d:%d", &h, &m, &s);
                if (parsed < 2) break;
                ++i;
                if (parsed == 2) s = 0;
                cfg.dailyTimes.emplace_back(h, m, s);
            }
            if (cfg.dailyTimes.empty()) { cout << t(Str::ERROR_NO_DAILY_TIMES) << "\n"; return 1; }
            cfg.loop = true; cfg.maxLoops = -1;

        } else if ((arg == "--every" || arg == "-e") && i + 1 < nArgs) {
            string daysStr = args[++i];
            int h = 0, m = 0, s = 0;
            if (i + 1 < nArgs && args[i + 1][0] != '-') {
                int parsed = sscanf(args[i + 1].c_str(), "%d:%d:%d", &h, &m, &s);
                if (parsed >= 2) { if (parsed == 2) s = 0; ++i; }
            }
            cfg.everySpec = parseEverySpec(daysStr, h, m, s);
            if (cfg.everySpec.days.empty()) {
                char buf[256]; snprintf(buf, sizeof(buf), t(Str::ERROR_INVALID_EVERY), daysStr.c_str());
                cout << buf << "\n"; return 1;
            }
            cfg.useEvery = true; cfg.loop = true; cfg.maxLoops = -1;
            auto target = nextEveryTarget(cfg.everySpec);
            cfg.ms = chrono::duration_cast<chrono::milliseconds>(
                         target - chrono::system_clock::now()).count();
            if (cfg.ms <= 0) cfg.ms = 1000;

        } else if (arg == "--for" && i + 1 < nArgs) {
            const string& forVal = args[++i];
            long long parsed = parseTime(forVal);
            if (parsed > 0) {
                cfg.forMs  = parsed;
                cfg.useFor = true;
            } else {
                char buf[256];
                snprintf(buf, sizeof(buf), t(Str::ERROR_INVALID_FOR), forVal.c_str());
                cout << buf << "\n";
                return 1;
            }

        } else if ((arg == "--lang" || arg == "-la") && i + 1 < nArgs) {
            ++i; // bereits im Vor-Durchlauf verarbeitet

        } else if (arg == "--macro") {
            // Bereits vollständig behandelt; verbleibende Tokens überspringen.
            if (i + 1 < nArgs) ++i; // Subbefehl
            if (i + 1 < nArgs) ++i; // Name
            if (i + 1 < nArgs) ++i; // Args (bei "add")

        } else if (arg == "--version" || arg == "-v") {
            cout << PRG_VERSION << "\n"; return 0;

        } else if (arg == "--help" || arg == "-h") {
            cout << t(Str::USAGE_HEADER);
            if (!launchedFromExistingConsole()) {
                cout << "\n" << t(Str::PRESS_ANY_KEY) << "\n" << flush;
                _getch();
            }
            return 0;

        } else if (arg[0] == '-') {
            char buf[256];
            snprintf(buf, sizeof(buf), t(Str::ERROR_UNKNOWN_OPTION), arg.c_str());
            cout << buf << "\n"; return 1;

        } else if (!cfg.useAtTime) {
            long long possible = parseTime(arg);
            if (possible > 0) {
                if (cfg.ms > MAX_MS - possible) cfg.ms = MAX_MS;
                else cfg.ms += possible;
            } else if (isStandaloneUnit(arg)) {
                char buf[256];
                snprintf(buf, sizeof(buf), t(Str::ERROR_DETACHED_UNIT), arg.c_str());
                cout << buf << "\n"; return 1;
            } else if (cfg.soundFile.empty()) {
                cfg.soundFile = arg;
            }

        } else if (cfg.soundFile.empty()) {
            cfg.soundFile = arg;
        }
    }
    return -1; // weiter zum Timer
}

// ═══════════════════════════════════════════════════════════════════════════
// ── Startmeldung ───────────────────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════════════════

static void printStartMessage(const TimerConfig& cfg) {
    char buf[256];
    snprintf(buf, sizeof(buf), t(Str::STARTED), PRG_VERSION);
    cout << buf;

    if (cfg.useAtTime) {
        if (cfg.useAtDateTime) {
            snprintf(buf, sizeof(buf), t(Str::TIMER_AT_DATETIME),
                     cfg.atYear, cfg.atMonth, cfg.atDay,
                     cfg.atHour, cfg.atMinute, cfg.atSecond);
        } else {
            snprintf(buf, sizeof(buf), t(Str::TIMER_AT_TIME),
                     cfg.atHour, cfg.atMinute, cfg.atSecond);
        }
        cout << buf;
        auto   atWallTarget = chrono::system_clock::now() + chrono::milliseconds(cfg.ms);
        time_t atTargetT    = chrono::system_clock::to_time_t(atWallTarget);
        if (isTargetTomorrow(atTargetT))
            cout << t(Str::TOMORROW_SUFFIX);

    } else if (cfg.useDailyTimes) {
        cout << t(Str::TIMER_DAILY);

    } else if (cfg.useEvery) {
        string daysStr = formatEveryDays(cfg.everySpec);
        snprintf(buf, sizeof(buf), t(Str::TIMER_EVERY),
                 daysStr.c_str(), cfg.everySpec.hour, cfg.everySpec.minute, cfg.everySpec.second);
        cout << buf;

    } else {
        // Countdown: Dauer anzeigen.
        // Zielzeit (HH:MM:SS) nur bei < 24 Stunden: bei längeren Timern ohne Aussagekraft;
        // außerdem wäre localtime_s für weit-zukünftige time_t-Werte nicht zuverlässig.
        string timerStr = formatVerbleibend(cfg.ms / 1000);
        snprintf(buf, sizeof(buf), t(Str::TIMER_COUNTER), timerStr.c_str());
        cout << buf;

        if (cfg.ms < 86400000LL) {
            auto   targetWall = chrono::system_clock::now() + chrono::milliseconds(cfg.ms);
            time_t targetT    = chrono::system_clock::to_time_t(targetWall);
            tm     targetTm{};
            if (localtime_s(&targetTm, &targetT) == 0) {
                char tbuf[64];
                snprintf(tbuf, sizeof(tbuf), t(Str::TIMER_TARGET),
                         targetTm.tm_hour, targetTm.tm_min, targetTm.tm_sec);
                cout << tbuf;
            }
        }
    }

    if (cfg.asyncSound)
        cout << t(Str::ASYNC_SUFFIX);
    if (!cfg.focusWindow.empty()) {
        snprintf(buf, sizeof(buf), t(Str::FOCUS_TARGET),
                 toConsole(toWideArgv(cfg.focusWindow)).c_str());
        cout << buf;
    }
    if (!cfg.openFile.empty()) {
        snprintf(buf, sizeof(buf), t(Str::OPEN_TARGET),
                 toConsole(toWideArgv(cfg.openFile)).c_str());
        cout << buf;
    }
    cout << "\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// ── Vorab-Prüfungen ────────────────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════════════════

// Prüft vor dem Timer-Start, ob Fenster und Datei bereits erreichbar sind.
// Warnungen erscheinen einmalig nach der Startmeldung.
static void doPreChecks(const TimerConfig& cfg) {
    if (!cfg.focusWindow.empty() && !findWindowByTitle(cfg.focusWindow)) {
        char buf[256];
        snprintf(buf, sizeof(buf), t(Str::WINDOW_NOT_FOUND_WARN),
                 toConsole(toWideArgv(cfg.focusWindow)).c_str());
        cout << buf << "\n" << flush;
    }

    if (!cfg.openFile.empty()) {
        bool isUrl = cfg.openFile.rfind("http://", 0) == 0 ||
                     cfg.openFile.rfind("https://", 0) == 0;
        if (!isUrl) {
            try {
                if (!fs::exists(fs::path(cfg.openFile))) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), t(Str::FILE_NOT_FOUND_WARN),
                             toConsole(toWideArgv(cfg.openFile)).c_str());
                    cout << buf << "\n" << flush;
                }
            } catch (const fs::filesystem_error&) {
                // Pfad ungültig; openFileAfterTimer() gibt beim Ablauf eine präzise Meldung.
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// ── Interaktive Modi ───────────────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════════════════

static int runLiveClockMode() {
    system("cls");
    while (true) {
        auto   now   = chrono::system_clock::now();
        time_t tnow  = chrono::system_clock::to_time_t(now);
        tm     local{};
        localtime_s(&local, &tnow);

        char timebuf[32];
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &local);
        cout << "\r" << timebuf << flush;

        char titlebuf[48];
        snprintf(titlebuf, sizeof(titlebuf), "Teefax - %s", timebuf);
        SetConsoleTitleA(titlebuf);

        auto next = chrono::time_point_cast<chrono::seconds>(now) + chrono::seconds(1);
        this_thread::sleep_until(next);
    }
    return 0; // via Strg+C
}

static int runStopwatchMode() {
    char buf[256];
    snprintf(buf, sizeof(buf), t(Str::STARTED), PRG_VERSION);
    cout << buf << " (" << t(Str::STOPWATCH_LABEL) << ")\n";
    cout << t(Str::STOPWATCH_HINT) << "\n";

    auto  start        = chrono::steady_clock::now();
    bool  isPaused     = false;
    chrono::steady_clock::time_point pauseStart;
    long long frozenMs     = 0;
    long long lastTitleSec = -1;

    while (true) {
        if (_kbhit()) {
            int key = _getch();
            if (key == ' ' || key == 'p' || key == 'P') {
                if (!isPaused) {
                    isPaused   = true;
                    pauseStart = chrono::steady_clock::now();
                    frozenMs   = chrono::duration_cast<chrono::milliseconds>(
                                   pauseStart - start).count();
                } else {
                    start    += chrono::steady_clock::now() - pauseStart;
                    isPaused  = false;
                }
            } else if (key == 'r' || key == 'R') {
                auto now   = chrono::steady_clock::now();
                start      = now;
                pauseStart = now;
                isPaused   = true;
                frozenMs   = 0;
                lastTitleSec = -1;
            }
        }

        long long elapsedMs = isPaused
                                  ? frozenMs
                                  : chrono::duration_cast<chrono::milliseconds>(
                                        chrono::steady_clock::now() - start).count();
        long long elapsedSec = elapsedMs / 1000;
        int       cs         = static_cast<int>((elapsedMs % 1000) / 10);
        string    secStr     = formatVerbleibend(elapsedSec);

        if (elapsedSec != lastTitleSec) {
            lastTitleSec = elapsedSec;
            wstring titleW = L"Teefax - " + toWide(t(Str::STOPWATCH_LABEL)) + L" - "
                             + wstring(secStr.begin(), secStr.end());
            SetConsoleTitleW(titleW.c_str());
        }

        char timebuf[64];
        snprintf(timebuf, sizeof(timebuf), "%s.%02d", secStr.c_str(), cs);
        char dispbuf[128];
        snprintf(dispbuf, sizeof(dispbuf), t(Str::ELAPSED), timebuf);

        char linebuf[256];
        if (isPaused)
            snprintf(linebuf, sizeof(linebuf), "%s %s", dispbuf, t(Str::STOPWATCH_PAUSED));
        else
            snprintf(linebuf, sizeof(linebuf), "%s", dispbuf);
        cout << "\r" << linebuf << "            " << flush;

        if (isPaused)
            this_thread::sleep_for(chrono::milliseconds(10));
        else {
            // Nächste 10ms-Grenze relativ zum Startpunkt – keine Drift
            auto nextTick = start + chrono::milliseconds(((elapsedMs / 10) + 1) * 10);
            this_thread::sleep_until(nextTick);
        }
    }
    return 0; // via Strg+C
}

// ═══════════════════════════════════════════════════════════════════════════
// ── Fortschrittsbalken ─────────────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════════════════

// Berechnet die verfügbare Balkenbreite anhand der Konsolenbreite.
// Verhindert Zeilenumbrüche in schmalen Fenstern; gibt 0 zurück wenn kein Platz.
static int calcEffectiveBarWidth(int prefixLen, int maxBarWidth) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        int w = (csbi.srWindow.Right - csbi.srWindow.Left + 1) - prefixLen - 13;
        if (w < 0) return 0;
        return (w > maxBarWidth) ? maxBarWidth : w;
    }
    return maxBarWidth;
}

// Baut den Präfix-Text auf: ggf. Schleifenzähler + verbleibende Zeit + "morgen"-Suffix.
static string buildBarPrefix(bool loop, long long loopCount,
                             const string& verbleibendStr, time_t wallTargetT) {
    char   buf[128];
    string prefix;
    if (loop) {
        snprintf(buf, sizeof(buf), t(Str::LOOP_PREFIX), loopCount);
        prefix += buf;
    }
    snprintf(buf, sizeof(buf), t(Str::REMAINING), verbleibendStr.c_str());
    prefix += buf;
    if (wallTargetT != 0 && isTargetTomorrow(wallTargetT))
        prefix += t(Str::TOMORROW_SUFFIX);
    return prefix;
}

// Zeichnet einen Fortschrittsbalken. filled == total ergibt einen vollen Balken.
static void renderBar(const string& prefix, int filled, int total) {
    cout << "\r" << prefix;
    if (total > 0) {
        cout << " [";
        for (int i = 0; i < total; ++i) cout << (i < filled ? '#' : '-');
        cout << "]";
    }
    cout << "        " << flush;
}

// ═══════════════════════════════════════════════════════════════════════════
// ── Alarm und Aktionen nach Ablauf ─────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════════════════

// Spielt den Alarmton entsprechend der Konfiguration ab (mit Wiederholungen).
static void playAlarmSound(const TimerConfig& cfg) {
    for (long long r = 0; cfg.alarmRepeat == 0 || r < cfg.alarmRepeat; ++r) {
        if (!cfg.soundFile.empty()) {
            try {
                fs::path p(cfg.soundFile);
                if (!fs::exists(p) || !fs::is_regular_file(p)) {
                    char buf[512];
                    snprintf(buf, sizeof(buf), t(Str::AUDIO_NOT_FOUND),
                             toConsole(toWideArgv(cfg.soundFile)).c_str());
                    cout << buf << "\n";
                } else {
                    wstring widePath = toWideArgv(cfg.soundFile);
                    if (!widePath.empty()) {
                        UINT flags = SND_FILENAME | (cfg.asyncSound ? SND_ASYNC : SND_SYNC);
                        PlaySoundW(widePath.c_str(), NULL, flags);
                    } else {
                        char buf[512];
                        snprintf(buf, sizeof(buf), t(Str::AUDIO_PATH_ERROR),
                                 toConsole(toWideArgv(cfg.soundFile)).c_str());
                        cout << buf << "\n";
                    }
                }
            } catch (const fs::filesystem_error& e) {
                char buf[512];
                snprintf(buf, sizeof(buf), t(Str::FILE_SYSTEM_ERROR), e.what());
                cout << buf << "\n";
            }
        } else {
            if (cfg.asyncSound) {
                thread([](){
                    PlaySoundA(reinterpret_cast<LPCSTR>(sound_data), NULL, SND_MEMORY | SND_ASYNC);
                }).detach();
            } else {
                PlaySoundA(reinterpret_cast<LPCSTR>(sound_data), NULL, SND_MEMORY | SND_SYNC);
            }
        }
        if (cfg.alarmRepeat == 0 || r < cfg.alarmRepeat - 1) {
            // Nachlauf-Stille: hält den Audio-Pipeline aktiv, bis der BT-Kopfhörer
            // den Rest des Alarmtons aus seinem Puffer vollständig abgespielt hat.
            // SND_ASYNC blockiert nicht; das Programm kann normal weiterlaufen.
            // Die Stille endet nach 1 s von selbst, oder der nächste Tick-Loop-
            // BT-Prewarm ersetzt sie (bei --loop).
            if (!cfg.mute && !cfg.asyncSound)
                PlaySoundA(reinterpret_cast<LPCSTR>(silentWav().data()),
                           NULL, SND_MEMORY | SND_ASYNC);
            this_thread::sleep_for(chrono::seconds(cfg.alarmInterval));
        }
    }
}

// Führt --cmd, --open und --focus nach Ablauf des Timers aus.
// warnOnFail steuert, ob Fehlermeldungen fuer --open und --focus ausgegeben werden.
// In Schleifen-Zwischendurchlaeufen gilt false: die Aktion wird trotzdem versucht,
// eine Fehlermeldung erscheint aber erst beim letzten Durchlauf, um die Ausgabe
// nicht zu zerstueckeln. --cmd ist ausgenommen: dessen Ausgabe erscheint stets.
static void runPostActions(const TimerConfig& cfg, bool warnOnFail) {
    if (!cfg.cmdArg.empty())
        runConsoleCommand(cfg.cmdArg);

    if (!cfg.openFile.empty()) {
        bool ok = openFileAfterTimer(cfg.openFile);
        // openFileAfterTimer gibt Fehlermeldungen per \r aus; bei warnOnFail=false
        // bereits gedruckte Reste mit Leerzeichen ueberschreiben.
        if (!ok && !warnOnFail)
            cout << "\r                                                                " << flush;
    }

    if (!cfg.focusWindow.empty()) {
        if (!bringWindowToFront(cfg.focusWindow) && warnOnFail) {
            char buf[256];
            snprintf(buf, sizeof(buf), t(Str::WINDOW_NOT_FOUND_WARN),
                     toConsole(toWideArgv(cfg.focusWindow)).c_str());
            cout << "\r" << buf << "                                        " << flush;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// ── Audio-Vorwärmung ───────────────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════════════════

// Initialisiert das Audio-Subsystem synchron, damit die winmm-Geräteöffnung
// (100–500 ms auf Frischstarts) nicht in den ersten Voralarm-Beep fällt.
// Greift nur wenn --prealarm aktiv ist; ohne Voralarm übernimmt der
// BT-Prewarm kurz vor dem Hauptalarm die Codec-Aktivierung.
// Laeuft auch bei --mute: der Voralarm-Beep ist unabhaengig vom Hauptalarm-Mute
// und benoetigt denselben Treiber-Vorlauf, sonst kommt der erste Beep zu spaet.
static void doAudioPrewarm(const TimerConfig& cfg) {
    if (cfg.preAlarmSeconds > 0) {
        PlaySoundA(reinterpret_cast<LPCSTR>(tinyInitWav().data()),
                   NULL, SND_MEMORY | SND_SYNC);
        PlaySoundA(reinterpret_cast<LPCSTR>(silentWav().data()),
                   NULL, SND_MEMORY | SND_ASYNC);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// ── Haupttimer-Schleife ────────────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════════════════

// Führt den Timer aus (ggf. mehrere Durchläufe bei --loop / --daily / --every).
// Rückgabe: 0 bei normalem Abschluss; 1 bei Fehler (z. B. Zielzeit in Vergangenheit).
static int runTimerLoop(TimerConfig& cfg) {
    using namespace chrono;
    constexpr int BAR_WIDTH = 30;

    // TIME_CRITICAL nur im Normalmodus: verbessert Aufwach-Präzision ohne CPU-Last,
    // da der Thread nahezu die gesamte Zeit in sleep_until schläft.
    // Im --eco-Modus bleibt die Priorität bei ABOVE_NORMAL (aus main()).
    std::optional<PriorityGuard> prioGuard;
    if (!cfg.eco) prioGuard.emplace();

    // wallMode aendert sich nie zwischen Durchlaeufen - einmalig vor der Schleife bestimmen.
    const bool wallMode = cfg.useDailyTimes || cfg.useEvery || cfg.useAtTime;

    // --for: Gesamtstartzeit; steady_clock, damit NTP-Korrekturen keinen Einfluss haben.
    const auto forStart = steady_clock::now();

    // Hilfsfunktion: wuerde ein weiterer Countdown-Durchlauf die --for-Zeit ueberschreiten?
    // Wanduhr-Modi werden am Schleifenkopf gesondert behandelt (Zielzeit unbekannt bis zur Berechnung).
    auto forWouldStop = [&]() -> bool {
        if (!cfg.useFor || wallMode) return false;
        auto elapsed = duration_cast<milliseconds>(steady_clock::now() - forStart).count();
        return elapsed >= cfg.forMs;
    };

    do {
        // --for:
        // Countdown: ist die --for-Zeit bereits abgelaufen? Dann keinen weiteren Durchlauf starten.
        // Wanduhr:   liegt der naechste Zielzeitpunkt noch innerhalb der --for-Zeit?
        if (cfg.useFor) {
            auto forElapsed = duration_cast<milliseconds>(steady_clock::now() - forStart).count();
            if (!wallMode) {
                if (forElapsed >= cfg.forMs) break;
            } else {
                if (forElapsed >= cfg.forMs) break;
                // Naechsten Zielzeitpunkt vorausberechnen (non-destructive, kein Seiteneffekt).
                // Fuer --at mit weit zukunftigen Daten wird msToNext direkt aus
                // millisecondsUntilDateTime() bezogen, um system_clock-Ueberlauf zu vermeiden.
                long long msToNext = 0;
                if (cfg.useDailyTimes) {
                    auto nextTarget = nextDailyTarget(cfg.dailyTimes);
                    msToNext = duration_cast<milliseconds>(nextTarget - system_clock::now()).count();
                } else if (cfg.useEvery) {
                    auto nextTarget = nextEveryTarget(cfg.everySpec);
                    msToNext = duration_cast<milliseconds>(nextTarget - system_clock::now()).count();
                } else {
                    // --at --loop: Zeitangabe -> naechstes Vorkommen; Datumsangabe -> naechstes Jahr
                    if (!cfg.useAtDateTime) {
                        msToNext = millisecondsUntilTime(cfg.atHour, cfg.atMinute, cfg.atSecond);
                    } else {
                        int peekYear = cfg.atYear + (cfg.loopCount > 0 ? 1 : 0);
                        msToNext = millisecondsUntilDateTime(peekYear, cfg.atMonth, cfg.atDay,
                                                             cfg.atHour, cfg.atMinute, cfg.atSecond);
                    }
                }
                if (msToNext < 0) msToNext = 0;
                if (forElapsed + msToNext > cfg.forMs) {
                    cout << "\n" << t(Str::FOR_TARGET_OUTSIDE);
                    break;
                }
            }
        }

        if (cfg.loop && cfg.loopCount < std::numeric_limits<long long>::max())
            ++cfg.loopCount;

        // ── Zielzeitpunkt für diesen Durchlauf bestimmen ──────────────
        system_clock::time_point wallTarget;

        // Maximale Millisekunden, die sicher zu system_clock::now() addiert werden können.
        // MinGW: system_clock intern nanosekunden (int64_t), max ab 1970 ~292 Jahre → ~Jahr 2262.
        // Conservative 100 Jahre: system_clock::now() (2026) + 100y bleibt in jedem Fall
        // unterhalb von INT64_MAX Nanosekunden, unabhaengig von der Implementierung.
        constexpr long long WALL_SAFE_MS = 3'153'600'000'000LL; // 100 * 365 * 86400 * 1000

        if (cfg.useDailyTimes) {
            wallTarget = nextDailyTarget(cfg.dailyTimes);
        } else if (cfg.useEvery) {
            wallTarget = nextEveryTarget(cfg.everySpec);
        } else if (cfg.useAtTime) {
            long long nextMs;
            if (cfg.useAtDateTime && cfg.loopCount > 1) {
                // Jährliche Wiederholung (29. Feb: mktime normalisiert auf 1. März).
                cfg.atYear += 1;
                nextMs = millisecondsUntilDateTime(cfg.atYear, cfg.atMonth, cfg.atDay,
                                                   cfg.atHour, cfg.atMinute, cfg.atSecond);
            } else {
                nextMs = cfg.useAtDateTime
                             ? millisecondsUntilDateTime(cfg.atYear, cfg.atMonth, cfg.atDay,
                                                         cfg.atHour, cfg.atMinute, cfg.atSecond)
                             : millisecondsUntilTime(cfg.atHour, cfg.atMinute, cfg.atSecond);
            }
            if (nextMs == 0) { cout << t(Str::ERROR_NEXT_TIME); return 1; }
            cfg.ms = (nextMs > MAX_MS) ? MAX_MS : nextMs;
            // wallTarget: bei cfg.ms <= WALL_SAFE_MS korrekt setzbar.
            // Bei cfg.ms > WALL_SAFE_MS wuerde system_clock::now() + milliseconds(cfg.ms)
            // ueberlaufen (MinGW Nanosekunden-system_clock). wallTarget wird daher auf
            // WALL_SAFE_MS begrenzt; fuer die Restzeit-Berechnung im Tick-Loop wird
            // stattdessen cfg.ms mit steady_clock-Arithmetik genutzt.
            {
                long long safeMs = (cfg.ms > WALL_SAFE_MS) ? WALL_SAFE_MS : cfg.ms;
                wallTarget = system_clock::now() + milliseconds(safeMs);
            }
        }

        // Fuer nahe Wanduhr-Ziele (daily, every, --at < 100 Jahre): wallTarget-Differenz.
        // Fuer weit-zukuenftige --at-Daten: cfg.ms direkt (wallTarget ist begrenzt, taugt
        // nicht als Restzeit-Referenz).
        long long totalMsThisRound = (wallMode && cfg.ms <= WALL_SAFE_MS)
                                         ? duration_cast<milliseconds>(wallTarget - system_clock::now()).count()
                                         : cfg.ms;

        // Wanduhr-Ziel als time_t für die "morgen"-Anzeige im Balken
        time_t wallTargetT = wallMode ? system_clock::to_time_t(wallTarget) : 0;

        auto start = steady_clock::now();
        // 'end' als absoluter steady_clock-Zeitpunkt entfällt: würde bei
        // totalMsThisRound jenseits der Nanosekunden-Reichweite von steady_clock
        // (~292 Jahre) überlaufen. Countdown-Modus verwendet stattdessen
        // Elapsed-Time-Arithmetik (nowSteady - start).

        // Benutzerdefinierte Notiz für Fenstertitel vorbereiten (einmalig pro Durchlauf)
        wstring customMsgW;
        if (!cfg.customMsg.empty()) {
            customMsgW = toWideArgv(cfg.customMsg);
            if (customMsgW.size() > 30) customMsgW = customMsgW.substr(0, 30) + L"...";
        }

        // ── Tick-Schleife: Fortschrittsbalken und Voralarm ────────────
        long long       lastVerbleibendSec = -1;
        bool            soundPrewarmed     = false; // BT-Prewarm: einmalig pro Durchlauf
        bool            preAlarmStarted    = false; // Voralarm-WAV: einmalig pro Durchlauf
        vector<uint8_t> preAlarmWavBuf;             // hält Voralarm-Puffer am Leben (SND_ASYNC)

        while (true) {
            auto nowSteady = steady_clock::now();
            auto nowWall   = system_clock::now();

            long long verbleibendMs = 0;
            bool      done          = false;

            if (wallMode && cfg.ms <= WALL_SAFE_MS) {
                // Nahe Wanduhr-Ziele: wallTarget ist korrekt, NTP-Korrekturen wirken.
                verbleibendMs = duration_cast<milliseconds>(wallTarget - nowWall).count();
                if (verbleibendMs <= 0) done = true;
            } else {
                // Countdown-Arithmetik: fuer reine Countdown-Timer und fuer weit-zukuenftige
                // --at-Daten (cfg.ms > WALL_SAFE_MS), bei denen wallTarget bewusst begrenzt
                // wurde und daher nicht als Restzeit-Referenz taugt.
                long long steadyElapsedMs = duration_cast<milliseconds>(nowSteady - start).count();
                verbleibendMs = totalMsThisRound - steadyElapsedMs;
                if (verbleibendMs <= 0) done = true;
            }
            if (done) break;
            if (verbleibendMs < 0) verbleibendMs = 0;

            long long verbleibendSec = (verbleibendMs + 999) / 1000;

            if (verbleibendSec != lastVerbleibendSec) {
                lastVerbleibendSec = verbleibendSec;

                // BT-Vorwärmung kurz vor Ablauf (nur ohne Voralarm).
                // Mit aktivem Voralarm übernimmt die Voralarm-WAV die Codec-Aktivierung.
                if (!cfg.mute && !soundPrewarmed &&
                    cfg.preAlarmSeconds == 0 && verbleibendSec > 0 && verbleibendSec <= 2)
                {
                    soundPrewarmed = true;
                    PlaySoundA(reinterpret_cast<LPCSTR>(silentWav().data()),
                               NULL, SND_MEMORY | SND_ASYNC | SND_LOOP);
                }

                // Voralarm: einmalig pro Durchlauf aufbauen und starten.
                // Der WAV-Puffer enthält Stille (BT-Prewarm) gefolgt von allen Beeps
                // als kontinuierlichen Stream – kein SND_LOOP, endet nach letztem Beep.
                if (cfg.preAlarmSeconds > 0 && !preAlarmStarted &&
                    verbleibendSec > 0 && verbleibendSec <= static_cast<long long>(cfg.preAlarmSeconds) + 3)
                {
                    preAlarmStarted = true;
                    int beepCount = static_cast<int>(
                        min(verbleibendSec, static_cast<long long>(cfg.preAlarmSeconds)));
                    // Stille-Prewarm auf den ersten Sekundentick ausrichten.
                    // Mindest 500 ms: sichert BT-Codec-Aktivierung auch bei kurzen Timern.
                    int prewarmMs = static_cast<int>(
                        verbleibendMs - static_cast<long long>(beepCount) * 1000LL);
                    if (prewarmMs < 500) prewarmMs = 500;
                    preAlarmWavBuf = buildPreAlarmWav(beepCount, prewarmMs);
                    if (!preAlarmWavBuf.empty())
                        PlaySoundA(reinterpret_cast<LPCSTR>(preAlarmWavBuf.data()),
                                   NULL, SND_MEMORY | SND_ASYNC);
                }

                // Fortschrittsbalken aktualisieren
                long long  elapsedMs = totalMsThisRound - verbleibendMs;
                if (elapsedMs < 0) elapsedMs = 0;
                long double fraction = (totalMsThisRound > 0)
                                           ? static_cast<long double>(elapsedMs) / static_cast<long double>(totalMsThisRound)
                                           : 1.0L;
                if (fraction < 0.0L) fraction = 0.0L;
                if (fraction > 1.0L) fraction = 1.0L;

                string verbleibendStr = formatVerbleibend(verbleibendSec);
                string prefix         = buildBarPrefix(cfg.loop, cfg.loopCount,
                                               verbleibendStr, wallTargetT);
                // Fenstertitel einmal pro Sekunde
                {
                    wstring titleW = L"Teefax - " + wstring(verbleibendStr.begin(), verbleibendStr.end());
                    if (!customMsgW.empty()) titleW += L" | " + customMsgW;
                    SetConsoleTitleW(titleW.c_str());
                }

                int effBar = calcEffectiveBarWidth(static_cast<int>(prefix.size()), BAR_WIDTH);
                int filled = (effBar > 0)
                                 ? min(static_cast<int>(fraction * effBar), effBar) : 0;
                renderBar(prefix, filled, effBar);
            }

            // Bis zur nächsten Sekundengrenze schlafen.
            // Nahe Wanduhr-Ziele (cfg.ms <= WALL_SAFE_MS): wall-clock-basierter Schlaf;
            //   NTP-Korrekturen wirken; Obergrenze 1,5 s verhindert Einfrieren bei Sprüngen.
            // Countdown und weit-zukuenftige --at-Daten: steady_clock-relativer Schlaf,
            //   kein Ueberlauf, kein Drift.
            if (verbleibendSec >= 1) {
                if (wallMode && cfg.ms <= WALL_SAFE_MS) {
                    auto nextWallTick   = wallTarget - seconds(verbleibendSec - 1);
                    auto durationToTick = nextWallTick - system_clock::now();
                    if (durationToTick > milliseconds(1500))
                        durationToTick = milliseconds(1500);
                    if (durationToTick > milliseconds(0))
                        this_thread::sleep_until(steady_clock::now() +
                                                 duration_cast<milliseconds>(durationToTick));
                } else {
                    // Millisekunden bis zur nächsten vollen Sekundengrenze (immer 1..1000 ms).
                    // nowSteady + kleiner Wert bleibt weit unterhalb der steady_clock-Reichweite.
                    long long msUntilNextTick = verbleibendMs - (verbleibendSec - 1) * 1000LL;
                    this_thread::sleep_until(nowSteady + milliseconds(msUntilNextTick));
                }
            }
        }
        // ── Ende Tick-Schleife ────────────────────────────────────────

        // Voralarm-Stream explizit beenden: der WAV-Puffer ist rechnerisch auf das
        // Timer-Ende ausgelegt, läuft aber durch die Laufzeit von buildPreAlarmWav()
        // und dem PlaySoundA-Aufruf selbst um einige Millisekunden über das Ende
        // hinaus. PlaySoundA(nullptr, ...) stoppt jede laufende Wiedergabe sauber,
        // statt sie beim Programmende abrupt mitten in der Stille abzuschneiden
        // (Ursache des Knackens). Ohne aktiven Voralarm ist der Aufruf wirkungslos.
        if (preAlarmStarted) {
            PlaySoundA(nullptr, nullptr, 0); // Voralarm sauber stoppen (vermeidet Knacken)
            // Sofort Stille-Loop starten, damit der BT-Codec aktiv bleibt,
            // bis playAlarmSound() den eigentlichen Alarmton startet.
            if (!cfg.mute)
                PlaySoundA(reinterpret_cast<LPCSTR>(silentWav().data()),
                           NULL, SND_MEMORY | SND_ASYNC | SND_LOOP);
        }

        // Vollständiger Balken am Ende des Durchlaufs
        {
            string prefix = buildBarPrefix(cfg.loop, cfg.loopCount, "00:00", 0);
            int effBar    = calcEffectiveBarWidth(static_cast<int>(prefix.size()), BAR_WIDTH);
            renderBar(prefix, effBar, effBar);
        }

        // Zeilenumbruch: immer beim letzten Durchlauf; immer wenn --cmd folgt,
        // damit CMD_STARTED und Prozessausgabe auf eigenen Zeilen stehen.
        bool isLastIteration = !cfg.loop
                               || (cfg.maxLoops != -1 && cfg.loopCount >= cfg.maxLoops)
                               || forWouldStop();
        if (isLastIteration || !cfg.cmdArg.empty())
            cout << "\n" << flush;

        if (!cfg.mute) playAlarmSound(cfg);

        // Konsolenmodus wiederherstellen, damit Kindprozesse den Originalzustand erben.
        restoreConsoleMode();

        runPostActions(cfg, isLastIteration);

        // QuickEdit für den nächsten Durchlauf neu deaktivieren.
        // --cmd-Kindprozesse und restoreConsoleMode() schalten QuickEdit zurück;
        // bei --daily/--every ohne --cmd tut restoreConsoleMode() dasselbe.
        if (cfg.loop) disableQuickEdit();

        // Fenstertitel zurücksetzen, bevor die Benachrichtigung den Thread blockiert.
        SetConsoleTitleA("Teefax");
        if (cfg.showMessage) {
            wstring notifyText = toWide(t(Str::NOTIFY_MSG));
            if (!cfg.customMsg.empty())
                notifyText += L"\n\n" + toWideArgv(cfg.customMsg);
            showNotification(toWide(t(Str::NOTIFY_TITLE)), notifyText);
        }

    } while (cfg.loop
             && (cfg.maxLoops == -1 || cfg.loopCount < cfg.maxLoops)
             && !forWouldStop());

    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// ── Hauptprogramm ──────────────────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════════════════

int main(int argc, char* argv[])
{
    // Macht Konsolenausgaben flüssiger
    ios::sync_with_stdio(false); // deaktiviert Synchronisation von C++-Streams (cin, cout, cerr) mit C-Streams (stdin, stdout, stderr)
    cin.tie(nullptr); // löst Bindung von cin an cout (kein automatisches Flush von cout vor Eingaben)

    // Argumentliste aufbauen: Config-Defaults (INI) + Kommandozeile
    vector<string> args;
    loadConfigArgs(args);
    for (int i = 1; i < argc; ++i) args.push_back(argv[i]);

    // Sprache frühzeitig setzen (--macro-Meldungen nutzen sie bereits)
    detectLanguageFromArgs(args);

    // --macro: auswerten und bei Treffer sofort beenden
    {
        int result = handleMacroCommands(args);
        if (result >= 0) return result;
    }

    // Makro-Expansion: ersten passenden CLI-Makronamen ersetzen
    expandMacroInArgs(args, argc);

    // Systemprioritäten und Signal-Handler
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    // Timer-Auflösung: erst nach Parsing aktivieren (--eco kann sie deaktivieren).
    // optional<TimePeriodGuard> lebt bis zum Ende von main() und ruft
    // timeEndPeriod sauber über den Destruktor auf.
    std::optional<TimePeriodGuard> timeGuard;

    // QuickEdit deaktivieren (außer bei --help, --version, ohne Argumente)
    setupQuickEdit(args, argc);

    // Sprache erneut setzen: expandierte Makros und CLI überschreiben den INI-Wert
    detectLanguageFromArgs(args);

    // Ohne Argumente: Hilfe anzeigen
    if (argc < 2) {
        char buf[256];
        snprintf(buf, sizeof(buf), t(Str::STARTED), PRG_VERSION);
        cout << buf << ".\n\n" << t(Str::USAGE_HEADER);
        if (!launchedFromExistingConsole()) {
            cout << "\n" << t(Str::PRESS_ANY_KEY) << "\n" << flush;
            _getch();
        }
        return 0;
    }

    // Argumente parsen
    TimerConfig cfg;
    {
        int result = parseArguments(args, cfg);
        if (result >= 0) {
            restoreConsoleMode();
            return result;
        }
    }

    // Grundlegende Validierung
    if (!cfg.useAtTime && !cfg.useDailyTimes && !cfg.useEvery &&
        !cfg.showLiveTime && !cfg.showStopwatch && cfg.ms <= 0) {
        cout << t(Str::ERROR_NO_TIME) << "\n";
        restoreConsoleMode();
        return 1;
    }
    if (cfg.ms > MAX_MS) cfg.ms = MAX_MS;

    if (cfg.useFor && !cfg.loop) {
        cout << t(Str::ERROR_FOR_REQUIRES_LOOP) << "\n";
        restoreConsoleMode();
        return 1;
    }

    // Normalmodus: Timer-Auflösung auf 1 ms setzen und Power-Throttling-Schutz aktivieren.
    // Eco-Modus: beides deaktiviert; Windows-Standard (~15,6 ms) bleibt erhalten.
    if (!cfg.eco) {
        timeGuard.emplace();
        applyPowerThrottlingExemption();
    }

    if (cfg.noSleep) preventSleep(true);

    if (!cfg.showLiveTime && !cfg.showStopwatch)
        printStartMessage(cfg);
    doPreChecks(cfg);

    // Interaktive Modi laufen bis Strg+C
    if (cfg.showLiveTime)  return runLiveClockMode();
    if (cfg.showStopwatch) return runStopwatchMode();

    // Im Eco-Modus wurde timeBeginPeriod bewusst nicht aufgerufen;
    // die Warnung soll nur bei unerwarteten Fehlern im Normalmodus erscheinen.
    if (!cfg.eco && !g_timePeriodOk)
        fprintf(stderr, "%s\n", t(Str::WARN_TIMER_PERIOD));

    doAudioPrewarm(cfg);

    // Timer ausführen
    int result = runTimerLoop(cfg);

    if (result == 0) cout << t(Str::TIMER_ENDED);
    if (cfg.noSleep) preventSleep(false);
    restoreConsoleMode();

    return result;
}