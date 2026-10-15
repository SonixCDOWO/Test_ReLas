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

### Harness de shader na CPU (`tools/skintest`)
Roda um vertex shader traduzido na CPU com os dados reais de um draw:
```
LARECOMP_SAVE_MSL=1 LARECOMP_DUMP_SKIN=400 ./build/lasrecomp.exe extracted --play
./.venv/Scripts/python tools/skintest/make_harness.py build/glsl/<vs>_vs.glsl build/skintest.cpp
clang++ -O1 -std=c++17 -w -o build/skintest.exe build/skintest.cpp && ./build/skintest.exe build/skin
```
O `.cpp` gerado contém o shader do jogo: fica em `build/` e nunca vai para o git.

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
