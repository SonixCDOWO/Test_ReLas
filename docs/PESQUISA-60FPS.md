# Pesquisa: como outros ports chegaram a 60 fps

_15/10/2026. Contexto: o Let's Go avança a lógica um passo por quadro e não tem passo de tempo global
([PROGRESSO.md](PROGRESSO.md), item 5). Destravar o limite deixa o jogo em velocidade dobrada._

## As quatro técnicas que existem

| Técnica | Como funciona | Quem usa | Serve para o Let's Go? |
|---|---|---|---|
| **1. Só destravar o limite** | troca o intervalo de present de 2 para 1 | mods de 60 fps de Switch (patch `.pchtxt`/IPS no `nvnWindowSetPresentInterval`) | **Não**: a lógica dobra de velocidade. O GBAtemp confirma que os mods de 60 fps do Let's Go têm exatamente esse problema |
| **2. Motor já usa delta time** | o jogo calcula a física pelo tempo real do quadro; o port só destrava e corrige bugs que aparecem em fps alto | Sonic Unleashed Recompiled (o jogo original já era sem taxa fixa) | **Não**: medimos que o Let's Go não usa tempo do quadro |
| **3. Interpolação de transformações** (a mais usada em ports) | a lógica continua a 30 Hz; para cada quadro extra, o renderizador redesenha a cena com as **matrizes** (câmera, objetos, ossos) a meio caminho entre o quadro anterior e o atual | Ship of Harkinian (Ocarina of Time), Zelda64Recomp e outros com RT64, sm64ex | **Sim, é o caminho**, adaptado ao nosso nível (NVN em vez de display lists de N64) |
| **4. Geração de quadros por imagem** | cria o quadro do meio a partir de duas imagens prontas (fluxo óptico) | programas externos de geração de quadros, recursos de driver | Funciona sem mexer no jogo, mas a qualidade é pior (artefatos na interface e nos textos) e o atraso aumenta |

## Como a interpolação de transformações funciona nos ports

### Ship of Harkinian (código fonte decompilado de Ocarina of Time)
Arquivo `soh/soh/frame_interpolation.cpp`:
- A cada quadro de lógica, **grava uma árvore** de todas as operações de matriz (translate, rotate, multiplicar,
  push/pop), indexada pelo objeto do jogo e pela ordem das chamadas (`FrameInterpolation_StartRecord`,
  `FrameInterpolation_RecordOpenChild`).
- Para um quadro intermediário, **percorre a árvore nova e a anterior juntas** (`interpolate_branch`) e mistura os
  valores com um fator `step` (0,5 para 60 a partir de 30). Ângulos usam o caminho mais curto; diferença grande
  demais é tratada como corte (não mistura).
- Casos especiais: matrizes relativas ao ator (evitam o efeito "papel" em rotações de 180°), câmera que pode pedir
  "não interpolar" num corte de cena, objetos que aparecem/somem (desenhados sem mistura), rastros (ribbons) e
  **membros com skinning** (guarda pose anterior e atual dos vértices).
- Funciona porque eles têm o código fonte: chamam essas funções em cada lugar que monta uma matriz.

### RT64 (Zelda64Recomp, recompilação estática como a nossa)
- O renderizador roda **dois renderizadores**: um sincronizado com o jogo (que lê a memória no ritmo original) e outro
  que **reexecuta os draw calls** gravados, em resolução maior e em outra taxa, interpolando.
- Para casar o que é o mesmo objeto entre dois quadros, os patches do jogo inserem **comandos extras** no display list
  (`gEXMatrixGroup`, com um `id` e flags dizendo o que interpolar: posição, rotação, escala, vértices, textura...).
  Sem `id`, há modo automático pela ordem.
- Mudar o fps **não altera a física nem a velocidade** do jogo (relatos sobre o Zelda64Recomp).

### sm64ex (Super Mario 64)
Patch de 60 fps aplicado no código decompilado: lógica a 30 Hz, animações e objetos interpolados no caminho de
renderização.

## O que isso significaria no nosso runtime

Nós não temos o código fonte (como o SoH) nem display lists de N64 (como o RT64). Mas temos algo parecido com o RT64:
**todos os comandos NVN de cada quadro passam pelo runtime** (`runtime/nvn/cmdbuf.c`, `vulkan/exec.c`). Um protótipo:

1. **Gravar o quadro**: guardar a lista de draws do quadro N (programa, buffers, cópias dos uniform buffers).
2. **Casar draws entre N−1 e N**: pela ordem dentro do passe, programa e endereço do vertex buffer (como o modo
   automático do RT64).
3. **Quadro intermediário**: reexecutar os draws de N com os **uniform buffers misturados** entre N−1 e N:
   - `cb3` (mundo e câmera) e `cb4` (paleta de ossos) são matrizes: misturáveis;
   - cortes de câmera e objetos novos/sumidos: sem mistura (como no SoH).
4. Apresentar o intermediário entre os dois quadros reais: custa **um quadro de atraso** (~33 ms).

Riscos e incertezas:
- O casamento de draws pode falhar em cenas com muitos objetos, partículas e listas que mudam de ordem.
- Interface (menus, textos) e efeitos de tela cheia geralmente não devem ser interpolados.
- Dados calculados pela GPU (compute) e texturas animadas não se interpolam assim.
- Interpolar matrizes com perspectiva ou escala exige decompor (o RT64 tem modo `DECOMPOSE`).
- É trabalho grande e exploratório: semanas, não dias. Mas é a mesma família de técnica que os ports bem-sucedidos
  usam.

## Protótipo (15/10/2026): `LARECOMP_INTERP=1` / `jogar-60fps.bat`

Implementado em `runtime/nvn/vulkan/exec.c` (seção "frame interpolation") e `device.c` (`backend_present`):
- **Gravação**: as listas de comandos de cada quadro (com um hash do conteúdo), o estado de execução no início do
  quadro e uma cópia de cada uniform buffer (cbuf 3+) dos estágios de vértice e fragmento. Chave de cada cópia:
  estágio, binding, programa, vertex buffer 0, parâmetros do draw e a ocorrência dessa chave no quadro.
- **Reexecução** no present do quadro N: as mesmas listas a partir do estado inicial, pulando o que escreve na memória
  do jogo (compute, cópias para buffer e de buffer para textura, atualização inline de uniform, syncs, contadores).
  Cada uniform é a mistura palavra a palavra das cópias de N−1 e N, só para floats normais e sem salto grande (corte de
  câmera, texturas que rolam); o resto, e draws sem par no quadro anterior, ficam com o valor de N.
- **Apresentação**: guarda a imagem de N numa cópia, mostra o intermediário e, meio período depois, N.
- **Segurança**: sem intermediário quando alguma lista mudou desde a execução (o jogo reaproveitou a memória) ou
  quando o jogo destruiu texturas, pools ou programas no quadro (o estado gravado poderia apontar para eles).

Resultados (Pallet Town, `tools/repro.sh` andando com o analógico):
- **Funciona**: entre os quadros 1329 e 1330 o cenário anda 12 px; o intermediário está a **6 px** de cada um (medido
  por alinhamento das imagens). Personagem e Eevee também ficam no meio do caminho; menus aparecem corretos.
- **Estável**: 2.000 quadros sem erro, sem avisos da validação do Vulkan; intermediário em ~99% dos quadros; a lógica
  continua a 30 fps (o custo de GPU dobra: cada quadro é desenhado duas vezes).
- **Defeitos conhecidos**: uma linha branca fina perto de uma porta no intermediário (provavelmente duas peças de
  geometria, uma com par e outra sem); atraso de meio quadro (~17 ms) a mais.
- **Não testado**: batalhas, cenas, efeitos com partículas e compute, sombras de objetos que surgem/somem.

Para testar: `jogar-60fps.bat`, ou `LARECOMP_INTERP=1` + `LARECOMP_WINDOW_SHOT=<frame>` (grava `build/window_mid.png`,
o intermediário, e `build/window.png`, o quadro). O `LARECOMP_SHOT_AT` grava a imagem do jogo depois da reexecução,
ou seja, o intermediário.

## Fontes
- Ship of Harkinian, interpolação: [frame_interpolation.cpp](https://github.com/HarbourMasters/Shipwright/blob/develop/soh/soh/frame_interpolation.cpp)
- RT64: [repositório](https://github.com/rt64/rt64) e [comandos estendidos (rt64_extended_gbi.h)](https://github.com/rt64/rt64/blob/main/include/rt64_extended_gbi.h)
- Zelda64Recomp e alta taxa de quadros: [DSOGaming](https://www.dsogaming.com/news/the-legend-of-zelda-majoras-mask-just-got-an-unofficial-pc-port-with-support-for-high-framerates-widescreen-ultrawide-gyro-aim-autosaves-and-low-input-lag/), [Digital Foundry via Nintendo Life](https://www.nintendolife.com/news/2024/05/video-digital-foundry-takes-a-look-at-zelda-64-recompiled-majoras-mask)
- Sonic Unleashed Recompiled: [README](https://github.com/hedge-dev/UnleashedRecomp/blob/main/README.md) (seção "High Frame Rate Glitches")
- Mods de 60 fps no Switch (método do intervalo de present): [tutorial no GBAtemp](https://gbatemp.net/threads/how-to-make-60fps-ips-patch-for-nintendo-switch-game-ghidra-tutorial.625675/)
- Let's Go e 60 fps (velocidade dobrada): [GBAtemp](https://gbatemp.net/threads/request-lets-go-eevee-and-swsh-1-3-1-60-fps-mod.581515/)
- sm64ex: [Game Tech Wiki](https://emulation.gametechwiki.com/index.php/Super_Mario_64_Port), [VGC](https://videogameschronicle.com/news/mario-64s-pc-port-has-been-modded-to-run-at-60fps)
