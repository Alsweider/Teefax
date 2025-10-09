#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <regex>
#include <iomanip>
#include <ctime>
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

using namespace std;

//Umrechnung in Millisekunden
long long unitToMilliseconds(double value, const string& unitPart) {
    if (unitPart == "ms")                             return static_cast<long long>(value);
    else if (unitPart == "s"  || unitPart.empty())    return static_cast<long long>(value * 1000);
    else if (unitPart == "m")                         return static_cast<long long>(value * 60 * 1000);
    else if (unitPart == "h")                         return static_cast<long long>(value * 60 * 60 * 1000);
    else if (unitPart == "d")                         return static_cast<long long>(value * 24 * 60 * 60 * 1000);
    else if (unitPart == "w")                         return static_cast<long long>(value * 7 * 24 * 60 * 60 * 1000);
    else if (unitPart == "mo" || unitPart == "mon")   return static_cast<long long>(value * 30 * 24 * 60 * 60 * 1000);
    else if (unitPart == "y"  || unitPart == "yr")    return static_cast<long long>(value * 365 * 24 * 60 * 60 * 1000);
    else {
        cout << "Unbekannte Einheit: " << unitPart << "\n";
        return 0;
    }
}

//Mehrteilige Zeitangabe analysieren
long long parseTime(const string& arg) {
    regex pattern(R"((\d+(?:\.\d+)?)([a-zA-Z]*))");
    smatch match;
    string::const_iterator searchStart(arg.cbegin());
    long long totalMs = 0;

    while (regex_search(searchStart, arg.cend(), match, pattern)) {
        double value = stod(match[1].str());
        string unit = match[2].str();
        totalMs += unitToMilliseconds(value, unit);
        searchStart = match.suffix().first;
    }

    return totalMs;
}

//Hilfsfunktion: std::string -> std::wstring
wstring toWide(const string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
    return wstr;
}

//Berechnet Millisekunden bis zur nächsten angegebenen Uhrzeit (HH:MM:SS)
long long millisecondsUntilTime(int hour, int minute, int second = 0) {
    using namespace chrono;
    auto now = system_clock::now();
    time_t tnow = system_clock::to_time_t(now);
    tm local = *localtime(&tnow);

    local.tm_hour = hour;
    local.tm_min  = minute;
    local.tm_sec  = second;

    auto target = system_clock::from_time_t(mktime(&local));
    if (target <= now) target += hours(24); //nächste Uhrzeit morgen

    return duration_cast<milliseconds>(target - now).count();
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        cout << "Verwendung:\n"
             << "  teefax [<Zeit>] [Sounddatei] [--mute] [--loop [Anzahl]] [--alarm-repeat <Anzahl>] [--alarm-interval <Sekunden>] [--at HH:MM[:SS]]\n\n"
             << "Beispiele:\n"
             << "  teefax 5m\n"
             << "  teefax 10s --loop\n"
             << "  teefax --loop 5 3s\n"
             << "  teefax --at 07:30\n"
             << "  teefax --at 07:30:15 \"C:\\Klang\\gong.wav\"\n";
        return 1;
    }

    string soundFile;
    bool mute = false;
    bool loop = false;
    int maxLoops = -1; //-1 = unendlich
    int alarmRepeat = 1;
    int alarmInterval = 2;
    bool useAtTime = false;
    int atHour = 0, atMinute = 0, atSecond = 0;
    long long ms = 0; //Zaehlerdauer in Millisekunden

    //Argumente parsen
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--mute" || arg == "-mute") mute = true;
        else if (arg == "--loop" || arg == "-loop") {
            loop = true;
            if (i + 1 < argc) {
                try {
                    int possibleCount = stoi(argv[i + 1]);
                    if (possibleCount > 0) {
                        maxLoops = possibleCount;
                        ++i;
                    }
                } catch (...) {}
            }
        }
        else if ((arg == "--alarm-repeat" || arg == "-ar") && i + 1 < argc) alarmRepeat = stoi(argv[++i]);
        else if ((arg == "--alarm-interval" || arg == "-ai") && i + 1 < argc) alarmInterval = stoi(argv[++i]);
        else if ((arg == "--at") && i + 1 < argc) {
            string timeStr = argv[++i];
            int parsed = sscanf(timeStr.c_str(), "%d:%d:%d", &atHour, &atMinute, &atSecond);
            if (parsed < 2) {
                cout << "Ungueltiges Zeitformat fuer --at. Erwartet HH:MM[:SS]\n";
                return 1;
            }
            if (parsed == 2) atSecond = 0;
            useAtTime = true;
            ms = millisecondsUntilTime(atHour, atMinute, atSecond);
        }
        else if (ms == 0 && !useAtTime) {
            ms = parseTime(arg);
            if (ms <= 0) {
                cout << "Ungueltige Zeitangabe: " << arg << "\n";
                return 1;
            }
        }
        else soundFile = arg;
    }

    if (!useAtTime && ms <= 0) {
        cout << "Bitte eine gueltige Zeit oder --at angeben.\n";
        return 1;
    }

    cout << "Teefax gestartet";
    if (useAtTime) cout << " fuer Uhrzeit " << setfill('0') << setw(2) << atHour << ":"
             << setw(2) << atMinute << ":" << setw(2) << atSecond;
    else cout << " mit Zaehler: " << ms/1000 << " Sekunden";
    cout << "\n";

    const int barWidth = 30;
    int loopCount = 0;

    do {
        if (loop) ++loopCount;
        auto start = chrono::steady_clock::now();
        auto end = start + chrono::milliseconds(ms);

        while (chrono::steady_clock::now() < end) {
            auto nowTime = chrono::steady_clock::now();
            auto verbleibendMs = chrono::duration_cast<chrono::milliseconds>(end - nowTime).count();
            long long verbleibendSec = (verbleibendMs + 999) / 1000;

            int totalSec = static_cast<int>(ms / 1000);
            int elapsedSec = totalSec - static_cast<int>(verbleibendSec);

            int filled = static_cast<int>((static_cast<double>(elapsedSec) / totalSec) * barWidth);
            if (filled > barWidth) filled = barWidth;

            int minutes = verbleibendSec / 60;
            int seconds = verbleibendSec % 60;

            cout << "\r";
            if (loop) cout << "Durchlauf " << loopCount << " | ";
            cout << "Verbleibend: " << setfill('0') << setw(2) << minutes << ":"
                 << setw(2) << seconds << " [";
            for (int i = 0; i < barWidth; ++i)
                cout << (i < filled ? '#' : '-');
            cout << "]   " << flush;

            auto nextTick = chrono::steady_clock::now() + chrono::milliseconds(500);
            this_thread::sleep_until(nextTick);
        }

        //Countdown bei 0
        cout << "\r";
        if (loop) cout << "Durchlauf " << loopCount << " | ";
        cout << "Verbleibend: 00:00 [";
        for (int i = 0; i < barWidth; ++i) cout << '#';
        cout << "]   " << flush;

        //Weckton
        if (!mute) {
            for (int r = 0; r < alarmRepeat; ++r) {
                if (!soundFile.empty()) {
                    wstring widePath = toWide(soundFile);
                    PlaySoundW(widePath.c_str(), NULL, SND_FILENAME | SND_SYNC);
                } else {
                    Beep(880, 300);
                    Beep(988, 300);
                    Beep(1047, 500);
                }
                if (r < alarmRepeat - 1) this_thread::sleep_for(chrono::seconds(alarmInterval));
            }
        }

        if (useAtTime) ms = millisecondsUntilTime(atHour, atMinute, atSecond);

    } while (loop && (maxLoops == -1 || loopCount < maxLoops));

    cout << "\nZaehler beendet.\n";
    return 0;
}
