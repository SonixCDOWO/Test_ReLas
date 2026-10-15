@echo off
rem Igual ao jogar.bat, com o prototipo de 60 fps por interpolacao ligado (docs/PESQUISA-60FPS.md): a logica do jogo
rem continua a 30 e um quadro intermediario e desenhado entre cada par. Experimental: pode ter defeitos visuais.
set LARECOMP_INTERP=1
call "%~dp0jogar.bat"
