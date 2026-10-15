@echo off
rem Roda o Pokemon Let's Go Eevee recompilado. O log vai para logs\jogo.log.
rem Roda a copia estavel em build\jogo (tools/publicar_jogo.sh), ou build\lasrecomp.exe se ela nao existir.
rem Opcoes (antes de rodar):  set LARECOMP_FPS=60   /   set LARECOMP_NPAD=right
cd /d "%~dp0"
set "PATH=C:\msys64\ucrt64\bin;%PATH%"
if not exist logs mkdir logs
set "EXE=build\lasrecomp.exe"
if exist build\jogo\lasrecomp.exe set "EXE=build\jogo\lasrecomp.exe"
echo Rodando o jogo (%EXE%)... (log em logs\jogo.log)
%EXE% extracted --play > logs\jogo.log 2>&1
echo O jogo fechou (codigo %ERRORLEVEL%).
pause
