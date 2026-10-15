# Progresso

Estado atual do port. A história completa, com cada problema e como foi resolvido, está em
[HISTORICO.md](HISTORICO.md). As ferramentas de diagnóstico estão em [DEPURACAO.md](DEPURACAO.md).

_Atualizado em 15/10/2026._

## O que funciona
| Área | Estado |
|---|---|
| Boot | completo: 4 módulos, SDK, construtores, threads, serviços do sistema |
| Controle | gamepad do PC como modo portátil (seleção de controle aceita) |
| Vídeo de abertura | imagem e áudio corretos, 1080p, termina e segue para o jogo |
| Áudio | música e efeitos (Opus via `hwopus`), áudio dos vídeos sem cortes |
| Ritmo | travado em 30 fps (nativo do jogo) |
| Introdução | idioma, Professor Carvalho, escolha de personagem, nome digitado corretamente |
| 3D | cenário correto (quarto, Pallet Town, céu), passes de profundidade/sombra |
| Personagens e Pokémon | modelos com esqueleto corretos, com animação (correção do `LDC.64` no tradutor de shaders) |
| Gameplay | andar, encontro com o Professor, captura do Pikachu (mira centralizada) |
| Save | salvar e carregar funcionam (`save/save_0/savedata.bin`) |

## O que falta
1. **Validar o 3D em mais cenas** (batalhas, Pokémon grandes, cidades): a correção dos modelos com esqueleto foi
   validada na Pallet Town. Histórico em [HISTORICO.md, parte 6](HISTORICO.md#parte-6-personagens-explodidos-resolvido).
2. **Mira por movimento**: o sensor responde sempre "parado". Opção futura: mirar com o analógico direito ou com
   o giroscópio do controle.
3. **Recursos online** (Pokémon GO, trocas, Poké Ball Plus): desligados de propósito (serviços offline).
4. **Desempenho**: não medido em gameplay longo; o vídeo de abertura e as cenas testadas ficam em 30 fps numa RTX 3080.

## Correções no runtime (resumo)
| Área | Correção | Arquivo |
|---|---|---|
| Kernel | contexto de exceção (`svcGetInfo` 20), Title ID do `main.npdm` | `runtime/kernel.c`, `main.c` |
| Kernel | condvar não readquire o mutex no timeout; não sobrescreve um "acordado" com timeout | `runtime/kernel.c` |
| Loader | 4 módulos (`main`, `sdk`, `subsdk0` glslc, `subsdk1` multimedia) | `runtime/loader.c`, `dispatch.c`, `gamelib.*` |
| Serviços | Bluetooth offline (`btm:u`, `bt`), `csrng`, `hwopus` | `runtime/services/offline.c`, `hwopus.c` |
| Vídeo | buffers do jogo no `GetOutputBuffer`, slots em rodízio, `FindInt64`/`SetCoreMask`, fim de stream `0x2000`, alocador do jogo | `runtime/hle/movie.c` |
| `vi` | `SetLayerScalingMode` | `runtime/hle/vi.c` |
| Controle | modo portátil, estilos suportados pelo jogo, sensor de movimento neutro | `runtime/hle/input.c`, `services/hid_set.c` |
| Teclado | nome em UTF-16 | `runtime/hle/swkbd.c` |
| NVN | programas por estágio, programas só com vertex shader, pacing pelo present interval | `runtime/nvn/cmdbuf.c`, `objects.c`, `vulkan/exec.c` |
| Tradutor de shaders | `LDC.64` cujo registrador de endereço também é destino (modelos com esqueleto) | `runtime/nvn/maxwell/translate.c` |
| Texturas | sem escala para texturas preenchidas pela CPU; invalidação por escrita do host | `runtime/nvn/vulkan/textures.c` |
| Áudio | fila mínima no `audout` + contagem de underruns | `runtime/services/audio.c` |
