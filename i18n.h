#ifndef I18N_H
#define I18N_H

// i18n.h
#pragma once
#include <string>
#include <unordered_map>
#include <cstdlib>   // getenv
#include <windows.h> // GetUserDefaultUILanguage

// ── Alle Texte als IDs ────────────────────────────────────────────────
enum class Str {
    // Statusmeldungen
    STARTED, TIMER_COUNTER, TIMER_AT_TIME, TIMER_DAILY,
    REMAINING, LOOP_PREFIX, TIMER_ENDED,
    ASYNC_SUFFIX, ERROR_NO_TIME, ERROR_PAST_DATETIME,
    ERROR_INVALID_AT, ERROR_INVALID_DAILY, ERROR_NO_DAILY_TIMES,
    ERROR_UNKNOWN_OPTION, ERROR_UNKNOWN_UNIT, ERROR_NEXT_TIME,
    ERROR_FILE_CONVERSION,
    ERROR_CMD_CONVERSION,
    NO_COMMAND,

    // Alarm / Benachrichtigung
    NOTIFY_TITLE, NOTIFY_MSG,

    // Datei / Befehl
    FILE_NOT_FOUND, FILE_OPENED, FILE_ERROR,
    CMD_EXECUTED, CMD_ERROR, CMD_TIMEOUT,
    AUDIO_NOT_FOUND, AUDIO_PATH_ERROR,
    FILE_SYSTEM_ERROR,

    // Hilfe
    USAGE_HEADER,

    TOMORROW_SUFFIX,
    _COUNT
};

// ── Übersetzungstabelle ───────────────────────────────────────────────
using TranslationMap = std::unordered_map<Str, const char*>;

static const TranslationMap LANG_DE = {
    { Str::STARTED,              "Teefax [v%s] gestartet" },
    { Str::TIMER_COUNTER,        " mit Zaehler: %s" },
    { Str::TIMER_AT_TIME,        " fuer Uhrzeit %02d:%02d:%02d" },
    { Str::TIMER_DAILY,          " mit taeglichem Alarm." },
    { Str::REMAINING,            "Verbleibend: %s" },
    { Str::LOOP_PREFIX,          "Durchlauf %d | " },
    { Str::TIMER_ENDED,          "\nZaehler beendet." },
    { Str::ASYNC_SUFFIX,         " (async sound)" },
    { Str::ERROR_NO_TIME,        "Bitte eine gueltige Zeit oder --at angeben." },
    { Str::ERROR_PAST_DATETIME,  "Datum/Zeit ungueltig oder Vergangenheit." },
    { Str::ERROR_INVALID_AT,     "Ungueltiges Zeitformat fuer --at." },
    { Str::ERROR_INVALID_DAILY,  "Ungueltige Uhrzeit: %s" },
    { Str::ERROR_NO_DAILY_TIMES, "Keine Uhrzeiten fuer --daily angegeben." },
    { Str::ERROR_UNKNOWN_OPTION, "Unbekannte Option: %s" },
    { Str::NOTIFY_TITLE,         "Teefax" },
    { Str::NOTIFY_MSG,           "Die Zeit ist verstrichen!" },
    { Str::FILE_NOT_FOUND,       "\nDatei nicht gefunden: %s" },
    { Str::FILE_OPENED,          "\nDatei geoeffnet: %s" },
    { Str::FILE_ERROR,           "\nFehler beim Oeffnen (Code %d): %s" },
    { Str::CMD_EXECUTED,         "\nBefehl ausgefuehrt: %s" },
    { Str::CMD_ERROR,            "\nFehler beim Ausfuehren (%d): %s" },
    { Str::ERROR_UNKNOWN_UNIT, "Unbekannte Einheit: %s" },
    { Str::ERROR_FILE_CONVERSION, "\nFehler bei der Pfad-Konvertierung: %s" },
    { Str::ERROR_CMD_CONVERSION, "\nFehler bei der Befehls-Konvertierung: %s" },
    { Str::NO_COMMAND, "\nKein Konsolenbefehl angegeben." },
    { Str::ERROR_NEXT_TIME, "\nFehler bei der Berechnung der naechsten Uhrzeit.\n" },
    { Str::AUDIO_NOT_FOUND,  "\nAudiodatei nicht gefunden: %s" },
    { Str::AUDIO_PATH_ERROR, "\nFehler bei Pfad-Konvertierung fuer Audiodatei: %s" },
    { Str::USAGE_HEADER,
        "Verwendung:\n"
        "  teefax [<Zeit>] [Sounddatei] [Optionen]\n\n"
        "Optionen:\n"
        "  -m,  --mute                 Kein Weckton abspielen\n"
        "  -l,  --loop [Anzahl]        Wiederhole den Timer\n"
        "  -ar, --alarm-repeat <n>     Anzahl der Weckton-Wiederholungen (Standard: 1)\n"
        "  -ai, --alarm-interval <s>   Abstand in Sekunden (Standard: 2)\n"
        "  -a,  --at HH:MM[:SS]        Starte bis zur angegebenen Uhrzeit\n"
        "  -a,  --at YYYY-MM-DD        Bis zum angegebenen Datum zaehlen\n"
        "  -a,  --at YYYY-MM-DD HH:MM  Datum und Uhrzeit kombiniert\n"
        "  -as, --async                Ton asynchron abspielen\n"
        "  -o,  --open <Dateipfad>     Datei nach Ablauf oeffnen\n"
        "  -c,  --cmd  <Befehl>        Konsolenbefehl nach Ablauf ausfuehren\n"
        "  -ns, --nosleep              Bildschirmschoner unterdruecken\n"
        "  -pa, --prealarm <s>         Sekuendlicher Beep X Sekunden vor Ablauf\n"
        "  -t,  --time                 Direktanzeige Datum & Zeit\n"
        "  -d,  --daily HH:mm[:ss]     Taeglicher Alarm\n"
        "  -la, --lang <Sprache>       Sprache festlegen (de, en, fr, pt, ru)\n\n"
        "Beispiele:\n"
        "  teefax 5m\n"
        "  teefax 10s --loop\n"
        "  teefax --loop 5 3s\n"
        "  teefax --at 07:30\n"
        "  teefax --at 2030-1-1\n"
        "  teefax --at 2030-1-1 20:15\n"
        "  teefax --at 07:30:15 \"C:\\Klang\\gong.wav\"\n"
        "  teefax 3s --async \"C:\\Klang\\gong.wav\"\n"
        "  teefax 10s --cmd \"shutdown /s /t 0\"\n"
        "  teefax 5m -c \"start notepad.exe\"\n"
        "  teefax 20s --prealarm 5\n"
        "  teefax --daily 4:00 10:00 16:00 22:00\n"
    },
    { Str::CMD_TIMEOUT, "\nBefehl laeuft noch, Programm wird fortgesetzt..." },
    { Str::FILE_SYSTEM_ERROR, "\nDateisystem-Fehler beim Oeffnen: %s" },
    { Str::TOMORROW_SUFFIX, " (morgen)" },
};

static const TranslationMap LANG_FR = {
    { Str::STARTED,              "Teefax [v%s] demarre" },
    { Str::TIMER_COUNTER,        " avec compteur: %s" },
    { Str::TIMER_AT_TIME,        " jusqu'a %02d:%02d:%02d" },
    { Str::TIMER_DAILY,          " avec alarme quotidienne." },
    { Str::REMAINING,            "Restant: %s" },
    { Str::LOOP_PREFIX,          "Boucle %d | " },
    { Str::TIMER_ENDED,          "\nCompteur termine." },
    { Str::ASYNC_SUFFIX,         " (son async)" },
    { Str::ERROR_NO_TIME,        "Veuillez indiquer une duree valide ou --at." },
    { Str::ERROR_PAST_DATETIME,  "Date/heure invalide ou dans le passe." },
    { Str::ERROR_INVALID_AT,     "Format d'heure invalide pour --at." },
    { Str::ERROR_INVALID_DAILY,  "Heure invalide: %s" },
    { Str::ERROR_NO_DAILY_TIMES, "Aucune heure indiquee pour --daily." },
    { Str::ERROR_UNKNOWN_OPTION, "Option inconnue: %s" },
    { Str::ERROR_UNKNOWN_UNIT,   "Unite inconnue: %s" },
    { Str::ERROR_NEXT_TIME,      "\nErreur lors du calcul de la prochaine heure.\n" },
    { Str::ERROR_FILE_CONVERSION,"\nErreur de conversion du chemin: %s" },
    { Str::ERROR_CMD_CONVERSION, "\nErreur de conversion de la commande: %s" },
    { Str::NO_COMMAND,           "\nAucune commande specifiee." },
    { Str::NOTIFY_TITLE,         "Teefax" },
    { Str::NOTIFY_MSG,           "Le temps est ecoule!" },
    { Str::FILE_NOT_FOUND,       "\nFichier introuvable: %s" },
    { Str::FILE_OPENED,          "\nFichier ouvert: %s" },
    { Str::FILE_ERROR,           "\nErreur a l'ouverture (code %d): %s" },
    { Str::CMD_EXECUTED,         "\nCommande executee: %s" },
    { Str::CMD_ERROR,            "\nErreur lors de l'execution (%d): %s" },
    { Str::AUDIO_NOT_FOUND,      "\nFichier audio introuvable: %s" },
    { Str::AUDIO_PATH_ERROR,     "\nErreur de conversion du chemin audio: %s" },
    { Str::USAGE_HEADER,
        "Utilisation:\n"
        "  teefax [<duree>] [fichier-son] [options]\n\n"
        "Options:\n"
        "  -m,  --mute                 Pas de son d'alarme\n"
        "  -l,  --loop [nombre]        Repeter le compteur\n"
        "  -ar, --alarm-repeat <n>     Repetitions de l'alarme (defaut: 1)\n"
        "  -ai, --alarm-interval <s>   Intervalle en secondes (defaut: 2)\n"
        "  -a,  --at HH:MM[:SS]        Compter jusqu'a l'heure indiquee\n"
        "  -a,  --at YYYY-MM-DD        Compter jusqu'a la date indiquee\n"
        "  -a,  --at YYYY-MM-DD HH:MM  Date et heure combinees\n"
        "  -as, --async                Jouer le son en mode asynchrone\n"
        "  -o,  --open <chemin>        Ouvrir un fichier apres le compteur\n"
        "  -c,  --cmd  <commande>      Executer une commande apres le compteur\n"
        "  -ns, --nosleep              Empecher la mise en veille\n"
        "  -pa, --prealarm <s>         Bip chaque seconde X secondes avant la fin\n"
        "  -t,  --time                 Affichage en direct de la date et l'heure\n"
        "  -d,  --daily HH:mm[:ss]     Alarme quotidienne\n"
        "  -la, --lang <langue>        Definir la langue (de, en, fr, pt, ru)\n\n"
        "Exemples:\n"
        "  teefax 5m\n"
        "  teefax 10s --loop\n"
        "  teefax --loop 5 3s\n"
        "  teefax --at 07:30\n"
        "  teefax --at 2030-1-1\n"
        "  teefax --at 2030-1-1 20:15\n"
        "  teefax --at 07:30:15 \"C:\\Sons\\gong.wav\"\n"
        "  teefax 3s --async \"C:\\Sons\\gong.wav\"\n"
        "  teefax 10s --cmd \"shutdown /s /t 0\"\n"
        "  teefax 5m -c \"start notepad.exe\"\n"
        "  teefax 20s --prealarm 5\n"
        "  teefax --daily 4:00 10:00 16:00 22:00\n"
    },
    { Str::CMD_TIMEOUT, "\nCommande toujours en cours, on continue..." },
    { Str::FILE_SYSTEM_ERROR, "\nErreur systeme de fichiers: %s" },
    { Str::TOMORROW_SUFFIX, " (demain)" },
    };

static const TranslationMap LANG_PT = {
    { Str::STARTED,              "Teefax [v%s] iniciado" },
    { Str::TIMER_COUNTER,        " com temporizador: %s" },
    { Str::TIMER_AT_TIME,        " ate as %02d:%02d:%02d" },
    { Str::TIMER_DAILY,          " com alarme diario." },
    { Str::REMAINING,            "Restante: %s" },
    { Str::LOOP_PREFIX,          "Ciclo %d | " },
    { Str::TIMER_ENDED,          "\nTemporizador terminado." },
    { Str::ASYNC_SUFFIX,         " (som async)" },
    { Str::ERROR_NO_TIME,        "Indique uma duracao valida ou --at." },
    { Str::ERROR_PAST_DATETIME,  "Data/hora invalida ou no passado." },
    { Str::ERROR_INVALID_AT,     "Formato de hora invalido para --at." },
    { Str::ERROR_INVALID_DAILY,  "Hora invalida: %s" },
    { Str::ERROR_NO_DAILY_TIMES, "Nenhuma hora indicada para --daily." },
    { Str::ERROR_UNKNOWN_OPTION, "Opcao desconhecida: %s" },
    { Str::ERROR_UNKNOWN_UNIT,   "Unidade desconhecida: %s" },
    { Str::ERROR_NEXT_TIME,      "\nErro ao calcular a proxima hora.\n" },
    { Str::ERROR_FILE_CONVERSION,"\nErro na conversao do caminho: %s" },
    { Str::ERROR_CMD_CONVERSION, "\nErro na conversao do comando: %s" },
    { Str::NO_COMMAND,           "\nNenhum comando especificado." },
    { Str::NOTIFY_TITLE,         "Teefax" },
    { Str::NOTIFY_MSG,           "O tempo esgotou-se!" },
    { Str::FILE_NOT_FOUND,       "\nFicheiro nao encontrado: %s" },
    { Str::FILE_OPENED,          "\nFicheiro aberto: %s" },
    { Str::FILE_ERROR,           "\nErro ao abrir o ficheiro (codigo %d): %s" },
    { Str::CMD_EXECUTED,         "\nComando executado: %s" },
    { Str::CMD_ERROR,            "\nErro ao executar o comando (%d): %s" },
    { Str::AUDIO_NOT_FOUND,      "\nFicheiro de audio nao encontrado: %s" },
    { Str::AUDIO_PATH_ERROR,     "\nErro na conversao do caminho do audio: %s" },
    { Str::USAGE_HEADER,
        "Utilizacao:\n"
        "  teefax [<duracao>] [ficheiro-som] [opcoes]\n\n"
        "Opcoes:\n"
        "  -m,  --mute                 Sem som de alarme\n"
        "  -l,  --loop [numero]        Repetir o temporizador\n"
        "  -ar, --alarm-repeat <n>     Repeticoes do alarme (padrao: 1)\n"
        "  -ai, --alarm-interval <s>   Intervalo em segundos (padrao: 2)\n"
        "  -a,  --at HH:MM[:SS]        Contar ate a hora indicada\n"
        "  -a,  --at YYYY-MM-DD        Contar ate a data indicada\n"
        "  -a,  --at YYYY-MM-DD HH:MM  Data e hora combinadas\n"
        "  -as, --async                Reproduzir som de forma assincrona\n"
        "  -o,  --open <caminho>       Abrir ficheiro apos o temporizador\n"
        "  -c,  --cmd  <comando>       Executar comando apos o temporizador\n"
        "  -ns, --nosleep              Impedir a suspensao do sistema\n"
        "  -pa, --prealarm <s>         Bip por segundo X segundos antes do fim\n"
        "  -t,  --time                 Mostrar data e hora em tempo real\n"
        "  -d,  --daily HH:mm[:ss]     Alarme diario\n"
        "  -la, --lang <lingua>        Definir o idioma (de, en, fr, pt, ru)\n\n"
        "Exemplos:\n"
        "  teefax 5m\n"
        "  teefax 10s --loop\n"
        "  teefax --loop 5 3s\n"
        "  teefax --at 07:30\n"
        "  teefax --at 2030-1-1\n"
        "  teefax --at 2030-1-1 20:15\n"
        "  teefax --at 07:30:15 \"C:\\Sons\\gong.wav\"\n"
        "  teefax 3s --async \"C:\\Sons\\gong.wav\"\n"
        "  teefax 10s --cmd \"shutdown /s /t 0\"\n"
        "  teefax 5m -c \"start notepad.exe\"\n"
        "  teefax 20s --prealarm 5\n"
        "  teefax --daily 4:00 10:00 16:00 22:00\n"
    },
    { Str::CMD_TIMEOUT, "\nComando ainda em execucao, continuando..." },
    { Str::FILE_SYSTEM_ERROR, "\nErro no sistema de ficheiros: %s" },
    { Str::TOMORROW_SUFFIX, " (amanha)" },
    };

static const TranslationMap LANG_RU = {
    { Str::STARTED,              "Teefax [v%s] zapushchen" },
    { Str::TIMER_COUNTER,        " s tajmerom: %s" },
    { Str::TIMER_AT_TIME,        " do %02d:%02d:%02d" },
    { Str::TIMER_DAILY,          " s ezhednevnym signalom." },
    { Str::REMAINING,            "Ostalos': %s" },
    { Str::LOOP_PREFIX,          "Krug %d | " },
    { Str::TIMER_ENDED,          "\nTajmer zavershyon." },
    { Str::ASYNC_SUFFIX,         " (async zvuk)" },
    { Str::ERROR_NO_TIME,        "Ukazhite vremya ili --at." },
    { Str::ERROR_PAST_DATETIME,  "Data/vremya neverno ili v proshlom." },
    { Str::ERROR_INVALID_AT,     "Nevernyi format vremeni dlya --at." },
    { Str::ERROR_INVALID_DAILY,  "Nevernoe vremya: %s" },
    { Str::ERROR_NO_DAILY_TIMES, "Ne ukazano vremya dlya --daily." },
    { Str::ERROR_UNKNOWN_OPTION, "Neizvestnaya optsiya: %s" },
    { Str::ERROR_UNKNOWN_UNIT,   "Neizvestnaya edinitsa: %s" },
    { Str::ERROR_NEXT_TIME,      "\nOshibka vychisleniya sleduyushchego vremeni.\n" },
    { Str::ERROR_FILE_CONVERSION,"\nOshibka preobrazovaniya puti: %s" },
    { Str::ERROR_CMD_CONVERSION, "\nOshibka preobrazovaniya komandy: %s" },
    { Str::NO_COMMAND,           "\nKomanda ne ukazana." },
    { Str::NOTIFY_TITLE,         "Teefax" },
    { Str::NOTIFY_MSG,           "Vremya isteklo!" },
    { Str::FILE_NOT_FOUND,       "\nFajl ne najden: %s" },
    { Str::FILE_OPENED,          "\nFajl otkryt: %s" },
    { Str::FILE_ERROR,           "\nOshibka otkrytiya (kod %d): %s" },
    { Str::CMD_EXECUTED,         "\nKomanda vypolnena: %s" },
    { Str::CMD_ERROR,            "\nOshibka vypolneniya (%d): %s" },
    { Str::AUDIO_NOT_FOUND,      "\nAudiofajl ne najden: %s" },
    { Str::AUDIO_PATH_ERROR,     "\nOshibka preobrazovaniya puti audio: %s" },
    { Str::USAGE_HEADER,
        "Ispol'zovanie:\n"
        "  teefax [<vremya>] [zvukfajl] [optsii]\n\n"
        "Optsii:\n"
        "  -m,  --mute                 Bez zvuka\n"
        "  -l,  --loop [kolichestvo]   Povtorit' tajmer\n"
        "  -ar, --alarm-repeat <n>     Povtoreniya signala (po umolchaniyu: 1)\n"
        "  -ai, --alarm-interval <s>   Interval v sekundakh (po umolchaniyu: 2)\n"
        "  -a,  --at HH:MM[:SS]        Otschet do ukazannogo vremeni\n"
        "  -a,  --at YYYY-MM-DD        Otschet do ukazannoj daty\n"
        "  -a,  --at YYYY-MM-DD HH:MM  Data i vremya vmeste\n"
        "  -as, --async                Vosproizvodit' zvuk asinkronno\n"
        "  -o,  --open <put'>          Otkryt' fajl posle tajmera\n"
        "  -c,  --cmd  <komanda>       Vypolnit' komandu posle tajmera\n"
        "  -ns, --nosleep              Zapretit' son sistemy\n"
        "  -pa, --prealarm <s>         Bip kazhduyu sekundu za X sekund do kontsa\n"
        "  -t,  --time                 Pokazyvat' tekushchee vremya\n"
        "  -d,  --daily HH:mm[:ss]     Ezhednevnyj signal\n"
        "  -la, --lang <yazyk>         Ustanovit' yazyk (de, en, fr, pt, ru)\n\n"
        "Primery:\n"
        "  teefax 5m\n"
        "  teefax 10s --loop\n"
        "  teefax --loop 5 3s\n"
        "  teefax --at 07:30\n"
        "  teefax --at 2030-1-1\n"
        "  teefax --at 2030-1-1 20:15\n"
        "  teefax --at 07:30:15 \"C:\\Zvuki\\gong.wav\"\n"
        "  teefax 3s --async \"C:\\Zvuki\\gong.wav\"\n"
        "  teefax 10s --cmd \"shutdown /s /t 0\"\n"
        "  teefax 5m -c \"start notepad.exe\"\n"
        "  teefax 20s --prealarm 5\n"
        "  teefax --daily 4:00 10:00 16:00 22:00\n"
    },
    { Str::CMD_TIMEOUT, "\nKomanda eshche vypolnyaetsya, prodolzhaem..." },
    { Str::FILE_SYSTEM_ERROR, "\nOshibka fajlovoj sistemy: %s" },
    { Str::TOMORROW_SUFFIX, " (zavtra)" },
    };

static const TranslationMap LANG_EN = {
    { Str::STARTED,              "Teefax [v%s] started" },
    { Str::TIMER_COUNTER,        " with timer: %s" },
    { Str::TIMER_AT_TIME,        " for time %02d:%02d:%02d" },
    { Str::TIMER_DAILY,          " with daily alarm." },
    { Str::REMAINING,            "Remaining: %s" },
    { Str::LOOP_PREFIX,          "Loop %d | " },
    { Str::TIMER_ENDED,          "\nTimer finished." },
    { Str::ASYNC_SUFFIX,         " (async sound)" },
    { Str::ERROR_NO_TIME,        "Please provide a valid time or --at." },
    { Str::ERROR_PAST_DATETIME,  "Date/time invalid or in the past." },
    { Str::ERROR_INVALID_AT,     "Invalid time format for --at." },
    { Str::ERROR_INVALID_DAILY,  "Invalid time: %s" },
    { Str::ERROR_NO_DAILY_TIMES, "No times given for --daily." },
    { Str::ERROR_UNKNOWN_OPTION, "Unknown option: %s" },
    { Str::NOTIFY_TITLE,         "Teefax" },
    { Str::NOTIFY_MSG,           "Time is up!" },
    { Str::FILE_NOT_FOUND,       "\nFile not found: %s" },
    { Str::FILE_OPENED,          "\nFile opened: %s" },
    { Str::FILE_ERROR,           "\nError opening file (code %d): %s" },
    { Str::CMD_EXECUTED,         "\nCommand executed: %s" },
    { Str::CMD_ERROR,            "\nError running command (%d): %s" },
    { Str::ERROR_UNKNOWN_UNIT, "Unknown unit: %s" },
    { Str::ERROR_FILE_CONVERSION, "\nError converting path: %s" },
    { Str::ERROR_CMD_CONVERSION, "\nError converting command: %s" },
    { Str::NO_COMMAND, "\nNo command specified." },
    { Str::ERROR_NEXT_TIME, "\nError calculating next time.\n" },
    { Str::AUDIO_NOT_FOUND,  "\nAudio file not found: %s" },
    { Str::AUDIO_PATH_ERROR, "\nPath conversion error for audio file: %s" },
    { Str::USAGE_HEADER,
        "Usage:\n"
        "  teefax [<time>] [soundfile] [options]\n\n"
        "Options:\n"
        "  -m,  --mute                 No alarm sound\n"
        "  -l,  --loop [count]         Repeat the timer\n"
        "  -ar, --alarm-repeat <n>     Alarm repetitions (default: 1)\n"
        "  -ai, --alarm-interval <s>   Interval in seconds (default: 2)\n"
        "  -a,  --at HH:MM[:SS]        Count to specified time\n"
        "  -a,  --at YYYY-MM-DD        Count to specified date\n"
        "  -a,  --at YYYY-MM-DD HH:MM  Date and time combined\n"
        "  -as, --async                Play sound asynchronously\n"
        "  -o,  --open <filepath>      Open file after timer\n"
        "  -c,  --cmd  <command>       Run console command after timer\n"
        "  -ns, --nosleep              Suppress screensaver\n"
        "  -pa, --prealarm <s>         Beep every second X seconds before end\n"
        "  -t,  --time                 Live date & time display\n"
        "  -d,  --daily HH:mm[:ss]     Daily alarm\n"
        "  -la, --lang <language>      Set language (de, en, fr, pt, ru)\n\n"
        "Examples:\n"
        "  teefax 5m\n"
        "  teefax 10s --loop\n"
        "  teefax --loop 5 3s\n"
        "  teefax --at 07:30\n"
        "  teefax --at 2030-1-1\n"
        "  teefax --at 2030-1-1 20:15\n"
        "  teefax --at 07:30:15 \"C:\\Sounds\\gong.wav\"\n"
        "  teefax 3s --async \"C:\\Sounds\\gong.wav\"\n"
        "  teefax 10s --cmd \"shutdown /s /t 0\"\n"
        "  teefax 5m -c \"start notepad.exe\"\n"
        "  teefax 20s --prealarm 5\n"
        "  teefax --daily 4:00 10:00 16:00 22:00\n"
    },
    { Str::CMD_TIMEOUT, "\nCommand still running, continuing..." },
    { Str::FILE_SYSTEM_ERROR, "\nFilesystem error while opening: %s" },
    { Str::TOMORROW_SUFFIX, " (tomorrow)" },
    };

// ── Spracherkennung ───────────────────────────────────────────────────
inline const TranslationMap& detectLanguage() {
    // 1. Umgebungsvariable TEEFAX_LANG hat Vorrang (z.B. "de", "en")
    if (const char* env = getenv("TEEFAX_LANG")) {
        std::string s(env);
        if (s == "de") return LANG_DE;
        if (s == "fr") return LANG_FR;
        if (s == "pt") return LANG_PT;
        if (s == "ru") return LANG_RU;
        if (s == "en") return LANG_EN;
    }
    // 2. Windows-Systemsprache
    LANGID id = GetUserDefaultUILanguage() & 0xFF; // Primärsprache
    if (id == LANG_FRENCH)     return LANG_FR;
    if (id == LANG_PORTUGUESE) return LANG_PT;
    if (id == LANG_RUSSIAN)    return LANG_RU;
    if (id == LANG_ENGLISH)    return LANG_EN;
    return LANG_DE; // Fallback
}

// ── Globale Instanz + Zugriffsfunktion ────────────────────────────────
inline const TranslationMap& currentLang() {
    static const TranslationMap& lang = detectLanguage();
    return lang;
}

// t() = translate – liefert den Rohstring für snprintf/printf
inline const char* t(Str id) {
    const auto& m = currentLang();
    auto it = m.find(id);
    return it != m.end() ? it->second : "???";
}

#endif // I18N_H
