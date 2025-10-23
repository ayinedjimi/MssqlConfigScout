@echo off
echo Compilation de MssqlConfigScout...
cl.exe /EHsc /W4 /std:c++17 /Fe:MssqlConfigScout.exe MssqlConfigScout.cpp /link comctl32.lib odbc32.lib user32.lib gdi32.lib
if %ERRORLEVEL% EQU 0 (
    echo Compilation reussie!
    echo Executable: MssqlConfigScout.exe
) else (
    echo Erreur de compilation.
)
pause
