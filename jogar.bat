@echo off
rem Roda o Pokemon Let's Go Eevee recompilado. O log vai para logs\jogo.log.
rem Opcoes (antes de rodar):  set LARECOMP_FPS=60   /   set LARECOMP_NPAD=right
cd /d "%~dp0"
set "PATH=C:\msys64\ucrt64\bin;%PATH%"
if not exist logs mkdir logs
echo Rodando o jogo... (log em logs\jogo.log)
build\lasrecomp.exe extracted --play > logs\jogo.log 2>&1
echo O jogo fechou (codigo %ERRORLEVEL%).
pause
