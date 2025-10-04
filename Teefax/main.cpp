#include <iostream>
#include <chrono>
#include <thread>
#include <windows.h> //Für Beep() und Sleep()

using namespace std;

int main(int argc, char* argv[])
{
    if (argc < 2) {
        cout << "Verwendung: timer <Sekunden>\n";
        return 1;
    }

    int sekunden = atoi(argv[1]);
    if (sekunden <= 0) {
        cout << "Bitte eine positive Zahl angeben.\n";
        return 1;
    }

    cout << "Timer gestartet: " << sekunden << " Sekunden\n";

    for (int i = sekunden; i > 0; --i) {
        cout << "\rVerbleibend: " << i << " s   " << flush;
        this_thread::sleep_for(chrono::seconds(1));
    }

    cout << "\rZeit abgelaufen!            \n";
    Beep(880, 300);  //Ton A5 für 300 ms
    Beep(988, 300);  //Ton H5 für 300 ms
    Beep(1047, 500); //Ton C6 für 500 ms

    return 0;
}
