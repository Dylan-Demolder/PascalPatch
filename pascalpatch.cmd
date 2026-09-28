@echo off
rem PascalPatch: with no arguments opens the app; otherwise runs the pascalpatch command line.
rem A release download carries its own Python in python\; a source checkout uses the installed one.
set "PYTHONPATH=%~dp0host\src;%PYTHONPATH%"
set "PP_PYTHON=python"
if exist "%~dp0python\python.exe" set "PP_PYTHON=%~dp0python\python.exe"
where "%PP_PYTHON%" >nul 2>nul || if not exist "%PP_PYTHON%" (
  echo PascalPatch needs Python 3.11 or newer. Install it from https://www.python.org/downloads/
  echo and tick "Add python.exe to PATH", or download the PascalPatch release zip, which includes Python.
  pause
  exit /b 1
)
if "%~1"=="" (
  "%PP_PYTHON%" -m pascalpatch.cli --root "%~dp0." app
) else (
  "%PP_PYTHON%" -m pascalpatch.cli --root "%~dp0." %*
)
