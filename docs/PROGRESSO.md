# Progresso

## 2026-10-14

### Onde o jogo chega
- Boot completo: módulos carregados, SDK e construtores do `main`, threads, serviços do sistema.
- Vulkan (RTX 3080) + NVN: a tela de seleção de controle renderiza corretamente.
- Controle: o gamepad do PC aparece como **modo portátil** (`LARECOMP_NPAD`). Seleção e confirmação funcionam.
- Tela de título: o vídeo de abertura toca com imagem e áudio corretos.
- Depois do vídeo: seleção de idioma, introdução do Professor Carvalho, escolha de personagem e entrada de nome funcionam.
- 3D renderiza (cenário, luz, geometria), mas os modelos animados saem "explodidos" (espinhos apontando para um ponto): skinning / matrizes de osso.
- FPS travado no nativo do jogo: **30 fps** (present interval 2).

### Correções no runtime (fork do LASRecomp)
| Problema | Correção | Arquivo |
|---|---|---|
| `svcGetInfo` 20 devolvia 0 → crash em `nn::os::SetDyingMessageRegion` | aloca o contexto de exceção | `runtime/kernel.c` |
| Title ID do Zelda fixo | lido do ACI0 do `main.npdm` (`0100187003A36000`) | `runtime/main.c`, `runtime/kernel.c` |
| `nn::vi::SetLayerScalingMode` ausente | HLE | `runtime/hle/vi.c` |
| `btm:u`, `bt`, `csrng` como stubs genéricos → abort no Bluetooth | serviços offline | `runtime/services/offline.c` |
| Condvar com timeout readquiria o mutex no kernel (o SDK já faz) → deadlock | não readquire no timeout | `runtime/kernel.c` |
| `BindProgram(NULL)` → crash | ignorado | `runtime/nvn/cmdbuf.c` |
| Pro Controller (FullKey) não aceito pelo jogo | controle como Handheld | `runtime/hle/input.c`, `runtime/services/hid_set.c` |
| `movie::MediaData::FindInt64`, `movie::SetCoreMask` ausentes | HLE | `runtime/hle/movie.c` |
| Texturas preenchidas pela CPU recebiam escala de resolução → overflow → device lost | ficam em tamanho cheio | `runtime/nvn/vulkan/textures.c`, `exec.c` |
| Escrita do host em memória sob texturas não invalidava o upload | `backend_pool_written` implementado | `runtime/nvn/vulkan/textures.c` |
| Buffers do decoder no heap do HLE | via alocador do jogo (`movie::SetAllocator`) | `runtime/hle/movie.c` |
| ~240 fps sem limite | pacing pelo present interval (`LARECOMP_FPS`) | `runtime/nvn/objects.c` |
| Áudio do vídeo robótico: o decoder reusava na hora o buffer de saída liberado, e o jogo ainda copiava dele | slots em rodízio | `runtime/hle/movie.c` |
| `hwopus` era stub: o jogo desistia do Opus e a saída de 6 canais ficava muda | serviço implementado com o Opus do FFmpeg | `runtime/services/hwopus.c` |
| Vídeo verde: o jogo passa o próprio buffer de quadro (NV12 compacto, w*h*3/2) no GetOutputBuffer e o runtime o trocava pelo seu | o quadro é copiado no buffer do jogo (layout compacto) | `runtime/hle/movie.c` |
| Vídeo travava num quadro aleatório: um waker rodava enquanto o timeout da condvar expirava e o kernel sobrescrevia o "acordado" (com o mutex já entregue) por "timeout"; o SDK pegava o mutex de novo e ficava esperando por ele mesmo | mantém o resultado de quem acordou | `runtime/kernel.c` |
| Jogo nunca saía do vídeo: no fim do arquivo o extrator devolvia 1 (erro genérico, "tente de novo") | devolve `0x2000`, o "fim de stream" do `movie` (de `NnMediaCommon::AndroidErrorToNnError`) | `runtime/hle/movie.c` |
| Sem 3D: o jogo liga programas separados por estágio (vertex e fragment em binds diferentes, NULL para desligar) e o runtime trocava o programa inteiro a cada bind → draws com meio programa, todos pulados | programa combinado por estágio (`bind_program_stages`) | `runtime/nvn/cmdbuf.c`, `runtime/nvn/vulkan/exec.c` |
| Margem curta na fila do `audout` | mínimo de `LARECOMP_AUDOUT_MS` (60 ms) e contagem de underruns | `runtime/services/audio.c` |

### Ferramentas de diagnóstico adicionadas
- `LARECOMP_THREAD_DUMP=<s>[,<repetir s>]`: lista as threads, o svc em que estão bloqueadas e a pilha do guest.
- Backtrace do crash com nomes `f_<endereço>` das funções do jogo; `svcBreak` mostra o `nn::Result`.
- `tools/addr2sym.py <endereço> [-d <bytes>]`: módulo, símbolo mais próximo e disassembly.
- `LARECOMP_LOG_BUF2TEX=1`: log das cópias buffer→textura dos planos de vídeo.
- `LARECOMP_MOVIE_TEST_FILL=<byte>`: preenche os quadros do vídeo com um valor (teste).
- `LARECOMP_LOG_MOVIE_CALLERS=1`: quem lê os quadros do vídeo (Buffer::Base, GetOutputBuffer).
- Estatística `[skip: ...]` no log a cada 2 s: draws pulados por motivo.
- `LARECOMP_MOVIE_STATE=1`: filas dos decoders a cada 5 s.
- `LARECOMP_AUDOUT_WAV=<prefixo>`: grava cada saída de áudio em `<prefixo>.<n>ch.raw`.

### Pendências
1. ~~Vídeo de título verde~~: resolvido (frame copiado no buffer que o jogo fornece).
2. ~~Áudio picotado/robótico~~: resolvido (rodízio de buffers). Música e efeitos do jogo via `hwopus`: implementado, falta validar em gameplay.
3. **Modelos 3D "explodidos"**: provável skinning (matrizes de osso / atributos de vértice / compute de skinning).
4. **~4.000 draws/2 s ainda pulados** (motivo "noobj"): provavelmente programas só com vertex shader (passes de profundidade/sombra), que o renderer não aceita.
5. **Nome no teclado**: "RRS" digitado aparece como "?S" (conversão de texto no swkbd).
6. Giroscópio (mirar a Poké Ball no modo portátil): `nn::hid::*SixAxisSensor*` não implementado.
