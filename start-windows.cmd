@echo off
setlocal
cd /d "%~dp0.."
if not exist "PlantsVsZombies.exe" goto missing
if not exist "pvzgame.exe" goto missing
if not exist "%~dp0hook\pvzmp-loader.exe" goto missing
if not exist "%~dp0hook\pvzmp-hook.dll" goto missing
if not exist "%~dp0install-menu.exe" goto missing
if not exist "%~dp0assets\menu.patch" goto missing
if not exist "%~dp0assets\multiplayer-stone.png" goto missing
if not exist "%~dp0assets\multiplayer-label.png" goto missing
set "SteamAppId=3590"
set "SteamGameId=3590"
set "PVZMP_TEST_INSTANCE="
echo Keep Steam running. Starting PvZ widescreen with the multiplayer mod...
"%~dp0hook\pvzmp-loader.exe" %*
if errorlevel 1 pause
exit /b
:missing
echo Install PvZWidescreenForSteam 1.0, then extract the mod into the game folder.
echo Expected: game\PlantsVsZombies.exe, game\pvzgame.exe, game\multiplayer\hook\pvzmp-loader.exe
pause
exit /b 1
