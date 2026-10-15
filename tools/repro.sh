#!/usr/bin/env bash
# repro.sh <nome> [frames a capturar] [VAR=valor ...]: reprodução determinística a partir do save.
#
# Carrega uma CÓPIA do save (save/ fica intocado) em build/test-save, ignora o controle real e aperta A só nos
# momentos mapeados (seleção de controle, confirmação, título, continuar). Do frame ~1150 em diante o jogo está no
# gameplay (Rota 1). Saída: logs/repro-<nome>.log e build/repro/<nome>/frame_N.png (+ contact.png).
# Um teste por vez: todos usam build/frames e build/test-save.
#
# Exemplo:  tools/repro.sh base 1300,1400
#           tools/repro.sh paleta 1300,1400 LARECOMP_VK_SKIN_IDENTITY=1
set -euo pipefail
cd "$(dirname "$0")/.."
export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"

name=${1:?nome do teste}
shots=${2:-1400}
shift $(( $# >= 2 ? 2 : 1 ))

rm -rf build/test-save build/frames "build/repro/$name"
mkdir -p build/test-save "build/repro/$name" logs
cp -r save/* build/test-save/

last=${shots##*,}
(   # the game itself is this background process (exec, no env wrapper): its Windows pid is the one killed below
    export LARECOMP_INPUT="400:A:6,500:A:6,1000:A:6,1100:A:6" LARECOMP_NO_USER_INPUT=1 \
        LARECOMP_SAVE_DIR=build/test-save LARECOMP_NO_AUDIO=1 LARECOMP_SHOT_AT="$shots"
    for kv in "$@"; do export "$kv"; done
    exec ./build/lasrecomp.exe extracted --play > "logs/repro-$name.log" 2>&1
) &
pid=$!
last_png=$(printf "build/frames/frame_%05d.png" "$last")
for _ in $(seq 1 240); do
    [ -f "$last_png" ] && break
    grep -q "FATAL\|exception c0" "logs/repro-$name.log" && { echo "CRASH: veja logs/repro-$name.log"; break; }
    sleep 1
done
sleep 1  # (the screenshot file is written; let the log catch up)
winpid=$(cat "/proc/$pid/winpid" 2>/dev/null || true)
if [ -n "$winpid" ]; then taskkill //F //T //PID "$winpid" >/dev/null 2>&1 || true; else kill "$pid" 2>/dev/null || true; fi
wait "$pid" 2>/dev/null || true
cp build/frames/*.png "build/repro/$name/" 2>/dev/null || true
FFMPEG=ffmpeg ./.venv/Scripts/python tools/contact_sheet.py "build/repro/$name/contact.png" build/repro/"$name"/frame_*.png >/dev/null
echo "build/repro/$name/contact.png"
