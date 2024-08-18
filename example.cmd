@echo off
setlocal

:: Check if a file or folder was dragged and dropped
if "%~1"=="" (
    set "archive=example"
) else (
    :: Extract the name of the dropped file or folder without the path
    set "archive=%~n1"
)

:: Test
if not exist .\%archive% pac_archiver.exe unpack %archive%.pac && echo "%archive%.pac" unpacked && pause && exit
pac_archiver.exe pack .\%archive% "%archive%.pac"

timeout /t 5
exit
