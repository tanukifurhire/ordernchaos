@echo off
setlocal enabledelayedexpansion

rem Always start in the batch file's folder
cd /d "%~dp0"
:clean_build
cd GGEngine || (echo ERROR: GGEngine not found.& pause & exit /b)
echo.

echo Cleaning build artifacts...
rmdir /s /q ".vs" 2>nul
rmdir /s /q "build" 2>nul
rmdir /s /q "out" 2>nul
rmdir /s /q "bin" 2>nul
rmdir /s /q "CMakeFiles" 2>nul
del *.sln 2>nul
del *.vcxproj* 2>nul
del CMakeCache.txt 2>nul
del cmake_install.cmake 2>nul

if exist ".vs" (
  attrib -r -s -h ".vs" /s /d 2>nul
  rmdir /s /q ".vs" 2>nul
)
if exist ".vs" (
  echo.
  echo WARNING: Could not delete .vs folder - it may be in use.
  echo Close Visual Studio and try again
  pause
  goto exit
)

echo Clean complete!
goto exit

:exit
endlocal
pause