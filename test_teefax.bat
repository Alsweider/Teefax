@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

set PASS=0
set FAIL=0
set EXE=teefax.exe

if not exist "%EXE%" (
    echo.
    echo  Fehler: %EXE% nicht gefunden.
    echo  Bitte dieses Skript im selben Ordner wie teefax.exe ausfuehren.
    echo.
    pause
    exit /b 1
)

echo.
echo  =======================================================
echo   Teefax Smoke-Tests
echo   Hinweis: teefax.ini sollte leer sein oder fehlen,
echo   damit INI-Einstellungen keine Tests verfaelschen.
echo  =======================================================
echo.

rem ── 1. Grundfunktionen ───────────────────────────────────────────────

set T=Timer 1s laeuft durch
"%EXE%" 1s --mute --nomsg >nul 2>&1
call :chk %errorlevel% 0

set T=Timer 500ms laeuft durch
"%EXE%" 500ms --mute --nomsg >nul 2>&1
call :chk %errorlevel% 0

set T=Kombinierte Zeit 1s500ms
"%EXE%" 1s500ms --mute --nomsg >nul 2>&1
call :chk %errorlevel% 0

set T=Kombinierte Zeit als separate Args (1s 500ms)
"%EXE%" 1s 500ms --mute --nomsg >nul 2>&1
call :chk %errorlevel% 0

set T=Kombinierte Zeit gemischt (1s30ms)
"%EXE%" 1s30ms --mute --nomsg >nul 2>&1
call :chk %errorlevel% 0

rem ── 2. Metafunktionen ────────────────────────────────────────────────

set T=--version gibt Exit 0 zurueck
"%EXE%" --version >nul 2>&1
call :chk %errorlevel% 0

set T=--version gibt Versionsnummer aus
for /f "delims=" %%v in ('"%EXE%" --version 2^>nul') do set VER=%%v
echo !VER! | findstr /r "[0-9][.][0-9]" >nul
call :chk %errorlevel% 0

set T=--help gibt Exit 0 zurueck
"%EXE%" --help >nul 2>&1
call :chk %errorlevel% 0

set T=Keine Argumente gibt Exit 0 zurueck (zeigt Hilfe)
"%EXE%" >nul 2>&1
call :chk %errorlevel% 0

rem ── 3. Fehlerbehandlung ──────────────────────────────────────────────

set T=Unbekannte Option gibt Exit 1 zurueck
"%EXE%" --xyzunknown >nul 2>&1
call :chk %errorlevel% 1

set T=Alleinstehende Einheit gibt Exit 1 (5 m)
"%EXE%" 5 m --mute --nomsg >nul 2>&1
call :chk %errorlevel% 1

set T=Alleinstehende Einheit gibt Exit 1 (10 sec)
"%EXE%" 10 sec --mute --nomsg >nul 2>&1
call :chk %errorlevel% 1

set T=Nullzeit gibt Exit 1 (0s)
"%EXE%" 0s --mute --nomsg >nul 2>&1
call :chk %errorlevel% 1

set T=Vergangenes Datum gibt Exit 1
"%EXE%" --at 1999-01-01 >nul 2>&1
call :chk %errorlevel% 1

set T=--daily ohne Zeiten gibt Exit 1
"%EXE%" --daily >nul 2>&1
call :chk %errorlevel% 1

set T=--daily mit ungueltigem Zeitformat gibt Exit 1
"%EXE%" --daily xyz >nul 2>&1
call :chk %errorlevel% 1

set T=--every mit ungueltigem Argument gibt Exit 1
"%EXE%" --every xyz >nul 2>&1
call :chk %errorlevel% 1

set T=--every mit gemischten Wochen- und Monatstagen gibt Exit 1
"%EXE%" --every mon,1 >nul 2>&1
call :chk %errorlevel% 1

rem ── 4. Optionen ──────────────────────────────────────────────────────

set T=--loop 2 Durchlaeufe
"%EXE%" 1s --loop 2 --mute --nomsg >nul 2>&1
call :chk %errorlevel% 0

set T=--for begrenzt Schleife (1s --loop --for 3s)
"%EXE%" 1s --loop --for 3s --mute --nomsg >nul 2>&1
call :chk %errorlevel% 0

set T=--for ohne --loop gibt Exit 1
"%EXE%" 1s --for 3s --mute --nomsg >nul 2>&1
call :chk %errorlevel% 1

set T=--for mit ungueltigem Wert gibt Exit 1
"%EXE%" 1s --loop --for xyz --mute --nomsg >nul 2>&1
call :chk %errorlevel% 1

set T=--loop mit Zaehler (--loop 3)
"%EXE%" 500ms --loop 3 --mute --nomsg >nul 2>&1
call :chk %errorlevel% 0

set T=--alarm-repeat 2
"%EXE%" 1s --alarm-repeat 2 --mute --nomsg >nul 2>&1
call :chk %errorlevel% 0

set T=--alarm-repeat mit ungueltigem Wert (abc) gibt Exit 0 (Fallback)
"%EXE%" 1s --alarm-repeat abc --mute --nomsg >nul 2>&1
call :chk %errorlevel% 0

set T=--alarm-repeat mit negativem Wert (-5) gibt Exit 0 (Fallback)
"%EXE%" 1s --alarm-repeat -5 --mute --nomsg >nul 2>&1
call :chk %errorlevel% 0

set T=--alarm-interval 1
"%EXE%" 1s --alarm-repeat 2 --alarm-interval 1 --mute --nomsg >nul 2>&1
call :chk %errorlevel% 0

set T=--nosleep
"%EXE%" 1s --mute --nomsg --nosleep >nul 2>&1
call :chk %errorlevel% 0

set T=--eco (Energiesparmodus, Exit 0)
"%EXE%" 1s --mute --nomsg --eco >nul 2>&1
call :chk %errorlevel% 0

set T=--msg mit Text
"%EXE%" 1s --mute --nomsg --msg "Testnotiz OK" >nul 2>&1
call :chk %errorlevel% 0

set T=--async (Ton spielt kurz, unhoerbar)
"%EXE%" 1s --async --nomsg >nul 2>&1
call :chk %errorlevel% 0

set T=--prealarm (3 Beeps sind normal)
"%EXE%" 4s --prealarm 3 --mute --nomsg >nul 2>&1
call :chk %errorlevel% 0

set T=--prealarm laenger als Timer (kein Absturz, Exit 0)
"%EXE%" 2s --prealarm 5 --mute --nomsg >nul 2>&1
call :chk %errorlevel% 0

set T=--cmd echo
"%EXE%" 1s --mute --nomsg --cmd "echo teefax_smoke_cmd_ok" >nul 2>&1
call :chk %errorlevel% 0

set T=--open nicht existente Datei (kein Absturz erwartet)
"%EXE%" 1s --mute --nomsg --open "C:\___teefax_nx___\nx.txt" >nul 2>&1
call :chk %errorlevel% 0

set T=--focus nicht vorhandenes Fenster (kein Absturz, Exit 0)
"%EXE%" 1s --mute --nomsg --focus "___teefax_nx___" >nul 2>&1
call :chk %errorlevel% 0

rem ── 5. Sprachen ──────────────────────────────────────────────────────

set T=--lang en
"%EXE%" 1s --mute --nomsg --lang en >nul 2>&1
call :chk %errorlevel% 0

set T=--lang de
"%EXE%" 1s --mute --nomsg --lang de >nul 2>&1
call :chk %errorlevel% 0

set T=--lang fr
"%EXE%" 1s --mute --nomsg --lang fr >nul 2>&1
call :chk %errorlevel% 0

set T=--lang pt
"%EXE%" 1s --mute --nomsg --lang pt >nul 2>&1
call :chk %errorlevel% 0

set T=--lang ru
"%EXE%" 1s --mute --nomsg --lang ru >nul 2>&1
call :chk %errorlevel% 0

rem ── 6. Konfigurationsdatei ───────────────────────────────────────────

set T=INI-Option --mute (Ton sollte fehlen)
echo --mute > teefax.ini
"%EXE%" 1s --nomsg 2>nul >nul
call :chk %errorlevel% 0
del teefax.ini >nul 2>&1

rem ── 7. Makro-System ──────────────────────────────────────────────────
rem  Laueft komplett in einer temporaeren INI, die danach geloescht wird.
rem  Voraussetzung: keine teefax.ini im Ordner (s. Hinweis oben).

set T=--macro add speichert Makro (Exit 0)
"%EXE%" --macro add ttest 1s --mute --nomsg >nul 2>&1
call :chk %errorlevel% 0

set T=--macro list zeigt Makro (Exit 0)
"%EXE%" --macro list >nul 2>&1
call :chk %errorlevel% 0

set T=--macro list gibt Makronamen aus
"%EXE%" --macro list 2>nul | findstr /i "ttest" >nul
call :chk %errorlevel% 0

set T=Makro-Expansion: ttest laeuft als Timer durch
"%EXE%" ttest >nul 2>&1
call :chk %errorlevel% 0

set T=--macro remove loescht Makro (Exit 0)
"%EXE%" --macro remove ttest >nul 2>&1
call :chk %errorlevel% 0

set T=--msg-Wert wird nicht als Makro expandiert (Exit 1 bei Expansion)
"%EXE%" --macro add ttest dummy --xyzunknown >nul 2>&1
"%EXE%" 1s --mute --msg ttest --nomsg >nul 2>&1
call :chk %errorlevel% 0
"%EXE%" --macro remove ttest >nul 2>&1

set T=-c-Wert wird nicht als Makro expandiert (Exit 1 bei Expansion)
"%EXE%" --macro add ttest dummy --xyzunknown >nul 2>&1
"%EXE%" 1s --mute --nomsg -c ttest >nul 2>&1
call :chk %errorlevel% 0
"%EXE%" --macro remove ttest >nul 2>&1

set T=--macro remove nicht-vorhandenes Makro gibt Exit 1
"%EXE%" --macro remove ttest_nx >nul 2>&1
call :chk %errorlevel% 1

set T=--macro add ungueltiger Name (Sonderzeichen) gibt Exit 1
"%EXE%" --macro add bad-name 1s >nul 2>&1
call :chk %errorlevel% 1

set T=--macro add ohne Argumente gibt Exit 1
"%EXE%" --macro add ttest >nul 2>&1
call :chk %errorlevel% 1

set T=--macro add ohne Name gibt Exit 1
"%EXE%" --macro add >nul 2>&1
call :chk %errorlevel% 1

rem Aufraumen: INI entfernen, falls vom Makro-Test erzeugt
if exist teefax.ini (
    findstr /v /i "^macro " teefax.ini >nul 2>&1
    if errorlevel 1 del teefax.ini >nul 2>&1
)

rem ── Ergebnis ─────────────────────────────────────────────────────────

echo.
echo  =======================================================
if %FAIL% gtr 0 (
    echo   ERGEBNIS: %PASS% bestanden  /  %FAIL% FEHLGESCHLAGEN
    echo  =======================================================
    echo.
    echo  Bitte die fehlgeschlagenen Tests manuell pruefen.
    echo.
    pause
    exit /b 1
) else (
    echo   ERGEBNIS: Alle %PASS% Tests bestanden.
    echo  =======================================================
    echo.
    pause
    exit /b 0
)

rem ── Hilfsfunktion ────────────────────────────────────────────────────
rem  Kein IF-Block, damit ) in %T% den Parser nicht verwirrt.
:chk
if "%1"=="%2" goto chk_pass
echo   [FAIL] %T%   (erwartet Exit %2  /  erhalten %1)
set /a FAIL+=1
exit /b 0
:chk_pass
echo   [PASS] %T%
set /a PASS+=1
exit /b 0