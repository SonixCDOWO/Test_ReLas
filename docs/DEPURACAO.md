# Ferramentas de depuração

Tudo o que foi usado para diagnosticar o port. As variáveis de ambiente valem para `build/lasrecomp.exe` (no Git Bash:
`LARECOMP_X=1 ./build/lasrecomp.exe extracted --play`).

## Execução e entrada
| Variável | Efeito |
|---|---|
| `LARECOMP_INPUT="frame:BOTAO:frames,..."` | entrada roteirizada (A, B, X, Y, L, R, PLUS, MINUS, direções, STICK_*). Desliga o controle real durante o teste |
| `LARECOMP_NPAD=handheld\|fullkey\|dual\|left\|right` | como o controle do PC aparece para o jogo (padrão: `handheld`) |
| `LARECOMP_FPS=<n>` | força n fps (padrão: o intervalo do jogo, 30 fps; `0` = sem limite) |
| `LARECOMP_NO_AUDIO=1` | sem áudio (testes automáticos) |
| `LARECOMP_SWKBD_UTF8=1` | teclado devolve UTF-8 (comportamento do Zelda) |

## Travamentos e crashes
| Ferramenta | Uso |
|---|---|
| Backtrace do crash | funções do jogo aparecem como `f_<endereço>`; `svcBreak` mostra o `nn::Result` |
| `LARECOMP_THREAD_DUMP=<s>[,<repete s>]` | lista cada thread: svc em que está bloqueada, argumentos e pilha do guest |
| `LARECOMP_TRACE_SVC=1` | loga cada chamada de sistema (muito volume) |
| `tools/addr2sym.py <endereço> [-d <bytes>]` | módulo, símbolo mais próximo e disassembly de um endereço do guest |
| `nm build/lasrecomp.exe` | símbolos do runtime para endereços `lasrecomp+0x...` |

## Gráficos
| Variável | Efeito |
|---|---|
| `LARECOMP_SHOT_AT=<frame>,...` | salva o quadro do jogo em `build/frames/frame_N.png` |
| Estatística a cada 2 s (`[vk] t=...`) | draws, passes, dispatches, uploads e `[skip: ...]` com os draws pulados por motivo |
| `LARECOMP_VK_VALIDATION=1` + `VK_ADD_LAYER_PATH=C:\msys64\ucrt64\bin` | camada de validação do Vulkan (achou o overflow das texturas do vídeo) |
| `LARECOMP_VK_DEVLOST_TRACE=1` (+ `LARECOMP_SYNC_SUBMIT=1`) | últimas operações da GPU antes de um device lost |
| `LARECOMP_SAVE_MSL=1` | salva cada shader traduzido em `build/glsl/<addr>_{vs,fs}.glsl` |
| `LARECOMP_DEBUG_FRAME=<frame>` | lista todos os draws daquele quadro (programa, formatos de atributo, render targets) |
| `LARECOMP_LOG_SKIN=1` (+ `LARECOMP_DEBUG_VSCB=1`) | detalha draws com esqueleto: vértices, índices/pesos dos ossos, estatística da paleta |
| `LARECOMP_DUMP_SKIN=<n>` | grava as entradas do n-ésimo draw com esqueleto em `build/skin/` |
| `LARECOMP_VK_UBO_DIRECT=1` | uniforms lidos direto da memória do jogo na GPU (experimento) |
| `LARECOMP_VK_COPY_VERTICES=1` | copia vértices em vez de ler direto (experimento) |
| `LARECOMP_NO_SCALE=1` | desliga a escala de resolução interna |
| `LARECOMP_VK_SKIN_ONEBONE=1` | shaders com esqueleto usam só o 1º osso (experimento X4; foi o paliativo antes da correção) |
| `LARECOMP_VK_SKIN_IDENTITY=1` / `LARECOMP_VK_SKIN_IDX_X=1` / `LARECOMP_VK_SKIN_W=<expr>` | outros experimentos do [PLANO-3D.md](PLANO-3D.md) |
| `LARECOMP_DUMP_SKIN=1` + `LARECOMP_DUMP_SKIN_FRAME=<frame>` | grava o 1º draw com esqueleto a partir daquele frame em `build/skin/` |

### Harness de shader na CPU (`tools/skintest`)
Roda um vertex shader traduzido na CPU com os dados reais de um draw:
```
LARECOMP_SAVE_MSL=1 LARECOMP_DUMP_SKIN=400 ./build/lasrecomp.exe extracted --play
./.venv/Scripts/python tools/skintest/make_harness.py build/glsl/<vs>_vs.glsl build/skintest.cpp
clang++ -O1 -std=c++17 -w -o build/skintest.exe build/skintest.cpp && ./build/skintest.exe build/skin
```
O `.cpp` gerado contém o shader do jogo: fica em `build/` e nunca vai para o git. Se a pasta tiver um `gpu_pos.bin` (exportado por
`tools/rdc_draw.py`), cada vértice também mostra a posição que a GPU calculou.

### RenderDoc (o que a GPU realmente recebeu)
RenderDoc 1.46 em `C:\Program Files\RenderDoc`. O runtime dispara a captura sozinho quando roda dentro dele:
```
RENDERDOC=1 tools/repro.sh rdc 1310 LARECOMP_RENDERDOC_AT=1290 [LARECOMP_RENDERDOC_FRAMES=2]
```
Isso grava `build/rdc/capture_frameN.rdc` (4 frames seguidos por padrão, porque o jogo não desenha a cena inteira
em todo present) e `build/rdc-shaders.txt`, com o hash de cada SPIR-V e o endereço do shader no jogo. Para analisar,
use o Python do `qrenderdoc`. Ele trava na inicialização se achar o depurador Python do VS Code no perfil do
usuário, então aponte `USERPROFILE`/`HOME` para uma pasta vazia **fora** de `build/rdc` (o `repro.sh` apaga essa pasta):
```
H="$(cygpath -w "$PWD/build/rdc-home")"
RDC_DIR=D:/projects/letsgo-recomp/build/rdc OUT=D:/projects/letsgo-recomp/build/rdc/skin.txt   USERPROFILE="$H" HOME="$H" "/c/Program Files/RenderDoc/qrenderdoc.exe" --python D:/projects/letsgo-recomp/tools/rdc_skin.py
RDC=<captura.rdc> EID=<evento> OUT=<arquivo.txt> USERPROFILE="$H" HOME="$H"   "/c/Program Files/RenderDoc/qrenderdoc.exe" --python D:/projects/letsgo-recomp/tools/rdc_draw.py
```
- `rdc_skin.py`: para cada draw com esqueleto, mostra o shader, a faixa de NDC e de `w` e quantos vértices saem da tela.
- `rdc_draw.py`: um draw vértice a vértice (`a11`, `a12`, `gl_Position`), separando os vértices com `w` fora da
  mediana. Exporta `build/rdc/draw_<eid>/` no formato do harness (`verts.bin`, `cbN.bin`, `gpu_pos.bin`).

Não ligue debug info nos shaders para capturar: com milhares de shaders, o replay leva mais de 20 minutos só para abrir.

### Jogar enquanto se testa
`jogar.bat` roda a cópia estável em `build/jogo/`. `tools/publicar_jogo.sh` (com o jogo fechado) copia o build atual
para lá depois de validado. Assim, recompilar e testar em `build/` não esbarra no executável travado pelo jogo aberto.

## Vídeo e áudio
| Variável | Efeito |
|---|---|
| `LARECOMP_AUDOUT_WAV=<prefixo>` | grava cada saída de áudio em `<prefixo>.<n>ch.raw` (s16, 48 kHz) |
| `LARECOMP_AUDOUT_MS=<ms>` | fila mínima da saída de áudio (padrão 60) |
| `LARECOMP_MOVIE_STATE=1` | estado das filas dos decoders a cada 5 s |
| `LARECOMP_LOG_MOVIE_CALLERS=1` | quem lê os quadros do vídeo (Buffer::Base, GetOutputBuffer) |
| `LARECOMP_MOVIE_MAX=<w>x<h>` | reduz vídeos maiores que isso |
| `LARECOMP_MOVIE_TEST_FILL=<byte>` | preenche os quadros do vídeo com um valor (teste) |
| `LARECOMP_LOG_BUF2TEX=1` | log das cópias buffer → textura dos planos de vídeo |

Para comparar áudio com o original:
`ffmpeg -i <video>.webm -vn -f s16le -ar 48000 -ac 2 ref.raw` e correlacionar com a gravação (numpy no `.venv`).
