@echo off
rem PascalPatch: with no arguments opens the app; otherwise runs the pascalpatch command line.
set "PYTHONPATH=%~dp0host\src;%PYTHONPATH%"
if "%~1"=="" (
  python -m pascalpatch.cli --root "%~dp0." app
) else (
  python -m pascalpatch.cli --root "%~dp0." %*
)
