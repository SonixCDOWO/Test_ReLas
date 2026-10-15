#!/usr/bin/env bash
# publicar_jogo.sh: copia o build atual (build/lasrecomp.exe + build/lasgame.dll) para build/jogo/, a cópia que o
# jogar.bat roda. Assim dá para recompilar e testar em build/ enquanto o jogo está aberto (o exe em uso fica travado
# no Windows). Rode só com o jogo fechado e com um build que já foi validado (tools/repro.sh).
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/jogo
cp build/lasrecomp.exe build/jogo/lasrecomp.exe
cmp -s build/lasgame.dll build/jogo/lasgame.dll 2>/dev/null || cp build/lasgame.dll build/jogo/lasgame.dll
echo "build/jogo atualizado: $(date '+%d/%m %H:%M')" | tee build/jogo/versao.txt
