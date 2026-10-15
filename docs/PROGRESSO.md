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
4. **Desempenho**: 1080p interno (`LARECOMP_SCALE=1`, ou F1 → resolução 1x) fica estável em 30 fps numa RTX 3080, e a GPU
   aguenta ~60. Gameplay longo ainda não foi medido.
5. **60 fps de verdade**: a lógica do jogo anda um passo por quadro. Com `LARECOMP_FPS=60` ele chega ao mesmo estado
   no mesmo número de quadro em metade do tempo, ou seja, roda em velocidade dobrada (medido em 15/10/2026 com
   `tools/repro.sh`). 60 fps exigiria mudar o passo de tempo do próprio jogo. O que já se sabe:
   - O motor escolhe o intervalo de present no fim de quadro (`0x7000029e80`, função `0x7000029d30`):
     `clamp(config[+8] / (G − 1), 1, 5)`, com `G` = 16.666.667 ns (um vsync de 60 Hz, ponteiro em `0x70015fb438`). O
     resultado é 2 e só é aplicado uma vez (`SetPresentInterval 2`, via `0x70005e79c0`); mesmo pacejado a 60, o jogo não
     pede intervalo 1, então `config[+8]` é fixo (~33,3 ms), não uma medida.
   - O único valor perto de 33,3 ms achado nos dados do `main` e no heap (`33.412.440` em `0x727eeccb68`) não é esse:
     trocá-lo por 16.666.667 não muda nada.
   - O float 1/30 aparece uma vez (`0x7000f47428`), usado só numa função de interpolação (`0x7000611440`), não no laço.
   - O laço do aplicativo (`0x70000231a0`) chama a atualização (`0x700001d1c0`) e depois o fim de quadro por vtable.
   - **Não há passo de tempo global.** Varredura da memória no gameplay (dados do `main` e heap, frame 1300) a 30 e a
     60 fps, endereço a endereço: dos 9.238 floats perto de 1/30 a 30 fps, nenhum vira 1/60 a 60 fps (8.508 ficam
     idênticos: são dados); os valores perto de 1/60 que só aparecem a 60 fps têm o mesmo conteúdo a 30. O único
     inteiro perto de 33,3 ms também é fixo e não altera nada. Ou seja, cada sistema avança por quadro.
   - Conclusão: 60 fps na velocidade certa não sai de um patch. Os caminhos seriam gerar quadros intermediários
     (interpolação no runtime, sem vetores de movimento do jogo: artefatos, principalmente na interface) ou um
     programa externo de geração de quadros. Pesquisa das técnicas usadas por outros ports: [PESQUISA-60FPS.md](PESQUISA-60FPS.md).
   - **Protótipo de interpolação funcionando** (60 fps no launcher / `LARECOMP_INTERP=1`): reexecuta cada quadro com os
     uniforms misturados e mostra o intermediário. Validado andando na Pallet Town; batalhas e cenas ainda não.

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
| Texturas | `RGB5A1`/`RGBA4` convertidos para RGBA8 (ícones dos Pokémon) | `runtime/nvn/vulkan/textures.c` |
| Texturas | sem escala para texturas preenchidas pela CPU; invalidação por escrita do host | `runtime/nvn/vulkan/textures.c` |
| Áudio | fila mínima no `audout` + contagem de underruns | `runtime/services/audio.c` |
