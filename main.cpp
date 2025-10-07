#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <regex>
#include <windows.h> //Beep
#include <mmsystem.h> //PlaySound / .WAV-Ausgabe
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

//Millisekunden hübsch formatiert ausgeben
string formatTime(long long ms) {
    long long totalSeconds = ms / 1000;
    long long days    = totalSeconds / 86400; totalSeconds %= 86400;
    long long hours   = totalSeconds / 3600;  totalSeconds %= 3600;
    long long minutes = totalSeconds / 60;    totalSeconds %= 60;
    long long seconds = totalSeconds;

    ostringstream oss;
    if (days)    oss << days << "d ";
    if (hours)   oss << hours << "h ";
    if (minutes) oss << minutes << "m ";
    oss << seconds << "s";
    return oss.str();
}

//Hilfsfunktion: std::string -> std::wstring
wstring toWide(const string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
    return wstr;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        cout << "Verwendung:\n"
             << "  teefax <Zeit> [Sounddatei] [--mute] [--loop]\n\n"
             << "Beispiele:\n"
             << "  teefax 5m\n"
             << "  teefax 10s --loop\n"
             << "  teefax 1h30m\n"
             << "  teefax 2d12h \"C:\\Klang\\gong.wav\" --loop\n"
             << "  teefax 1mo15d --mute\n"
             << "  teefax 1y\n";
        return 1;
    }

    string zeitArg = argv[1];
    long long ms = parseTime(zeitArg);

    if (ms <= 0) {
        cout << "Bitte eine gültige Zeitangabe machen.\n";
        return 1;
    }

    string soundFile;
    bool mute = false;
    bool loop = false;

    for (int i = 2; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--mute" || arg == "-mute") mute = true;
        else if (arg == "--loop" || arg == "-loop") loop = true;
        else soundFile = arg;
    }

    cout << "Teefax gestartet: " << zeitArg
         << " (Gesamtdauer: " << formatTime(ms) << ")\n";

    do {
        auto start = chrono::steady_clock::now();
        auto end = start + chrono::milliseconds(ms);

        while (chrono::steady_clock::now() < end) {
            auto verbleibendMs = chrono::duration_cast<chrono::milliseconds>(end - chrono::steady_clock::now()).count();
            cout << "\rVerbleibend: " << formatTime(verbleibendMs) << "   " << flush;

            //präziser Schlaf bis zur nächsten Sekunde
            auto nextTick = chrono::steady_clock::now() + chrono::seconds(1);
            this_thread::sleep_until(nextTick);
        }

        cout << "\rZeit abgelaufen!            \n";

        if (!mute) {
            if (!soundFile.empty()) {
                wstring widePath = toWide(soundFile);
                PlaySoundW(widePath.c_str(), NULL, SND_FILENAME | SND_SYNC);
            } else {
                Beep(880, 300);
                Beep(988, 300);
                Beep(1047, 500);
            }
        }

    } while (loop);

    return 0;
}
