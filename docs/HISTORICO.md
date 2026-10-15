# Histórico do projeto: o que deu errado e como foi resolvido

Este documento conta, em ordem, como o port do **Pokémon Let's Go Eevee** saiu do zero até o jogo ser jogável
(começar um jogo novo, capturar o Pikachu, salvar e carregar). Cada problema aparece com o **sintoma** (o que se via),
o **diagnóstico** (como a causa foi encontrada), a **causa** e a **correção**, com os arquivos alterados.

Período: 14 e 15/10/2026. Estado atual e pendências: [PROGRESSO.md](PROGRESSO.md). Ferramentas de diagnóstico:
[DEPURACAO.md](DEPURACAO.md).

---

## Parte 1: escolha do caminho

### A ideia
Rodar o jogo do Switch **nativamente no PC**, sem emulador, usando **recompilação estática**: o código ARM64 do jogo
é traduzido para C uma única vez, compilado como um programa Windows, e um *runtime* substitui o sistema do Switch
(kernel, serviços, GPU, áudio, controles).

### Por que o LASRecomp como base
- O [LASRecomp](https://github.com/peiikai/LASRecomp) (Link's Awakening, Switch, licença MIT) já tinha provado que
  o caminho funciona no Switch: tradutor ARM64 → C, kernel/serviços do Horizon e a API gráfica **NVN** traduzida
  para Vulkan.
- A análise ([ANALISE-LASRecomp.md](ANALISE-LASRecomp.md)) mostrou que o núcleo é genérico e que o específico do
  Zelda fica em camadas separáveis (patches por endereço, randomizer, interface).

### Alternativas consideradas
| Alternativa | Decisão | Motivo |
|---|---|---|
| Começar um recompilador do zero | não | o LASRecomp já resolve a parte genérica |
| HWDER (Hyrule Warriors) | plano B para gráficos | recompila o driver NVN e emula comandos Maxwell; licença não declarada, só estudo |
| ssbu-recomp (Smash) | só estudo | GPL-3, incompatível com copiar código para um projeto MIT |
| mk8-recomp | descartado | depende do suyu em execução |
| Reestruturar com CMake (sugestão externa) | não | o Makefile do LASRecomp já funciona; reorganizar não aproxima o objetivo |

### Extração do jogo (regras)
- O jogo vem do **cartucho do próprio usuário** (XCI). Extração com `hactool` usando o `prod.keys` **gerado no
  console do usuário** (Lockpick_RCM). Chaves baixadas de sites foram recusadas: a verificação por hash mostrou que
  as primeiras chaves oferecidas eram idênticas ao pacote do ProdKeys.NET; o dump verdadeiro tinha hash diferente e
  chaves exclusivas do console (`device_key_4x`, `sd_seed`, ...).
- Resultado em `extracted/` (fora do git): `exefs/` (código), `romfs/` (assets) e `romfs.bin` (imagem RomFS crua,
  gerada pelo `hactool --romfs`).
- **Nada do jogo entra no repositório:** arquivos do jogo, chaves, código C gerado, logs, saves e caches estão no
  `.gitignore`. O harness de shader (`tools/skintest`) gera o código derivado do jogo só localmente.

---

## Parte 2: diagnóstico do binário

Detalhes em [DIAGNOSTICO.md](DIAGNOSTICO.md). Em resumo:

- 4 módulos: `main` (o jogo), `sdk` (bibliotecas da Nintendo), `subsdk0` (**glslc**, compilador de shaders) e
  `subsdk1` (**multimedia**, vídeo). O Zelda só tem `main` + `sdk`.
- O tradutor do LASRecomp converteu **100% das instruções** dos 4 módulos (0 não suportadas).
- Title ID confirmado no `main.npdm`: `0100187003A36000`.

---

## Parte 3: montar o fork

1. **Ambiente:** MSYS2 UCRT64 (clang, SDL3, FFmpeg, Vulkan, shaderc) instalado via winget/pacman; Python com
   capstone/unicorn/lz4 em `.venv`.
2. **Remoção da camada do Zelda:** 16 arquivos de HLE de gameplay/randomizer e 11 telas de interface removidos.
   O que o resto do runtime ainda chamava virou stub neutro em `runtime/hle/zelda_stubs.c`.
3. **4 módulos em vez de 2:** loader, tabela de despacho, biblioteca do jogo (`lasgame.dll`, ABI 2) e Makefile
   passaram a tratar `main`, `sdk`, `subsdk0` e `subsdk1` (`runtime/loader.c`, `dispatch.c`, `gamelib.*`,
   `tools/gamelib_entry.c`, `gamebuild.c`, `rt.h`).
4. **Gerador sem os patches do Zelda:** `tools/patches.py` esvaziado (os patches eram endereços do executável do Zelda).

Primeiro build: runtime em segundos; código do jogo (~550 MB de C) em ~22 minutos.

---

## Parte 4: do boot até a tela de título

| # | Sintoma | Diagnóstico | Causa | Correção | Arquivo |
|---|---|---|---|---|---|
| 1 | Crash nos construtores do `main` (acesso a `0x1c0`) | disassembly: `nn::os::SetDyingMessageRegion` grava em `[GetInfo(20)] + 0x1c0` | `svcGetInfo` id 20 (contexto de exceção do usuário) devolvia 0 | aloca uma área e devolve o endereço | `runtime/kernel.c` |
| 2 | — (encontrado no caminho) | código do `svcGetInfo` | Title ID do Zelda fixo no kernel | lido do ACI0 do `main.npdm` | `runtime/main.c`, `kernel.c` |
| 3 | Crash em `nn::vi::GetService` | addr2sym no backtrace | `nn::vi::SetLayerScalingMode` sem HLE (o Zelda não usa) | HLE da função | `runtime/hle/vi.c` |
| 4 | Jogo aborta (`svcBreak`) | backtrace nomeado: `nn::btm::user::InitializeBtmUserInterface` | serviços `btm:u`/`bt` eram stubs genéricos sem objetos/eventos | serviços "offline" (Bluetooth sem dispositivos) + `csrng` com números aleatórios | `runtime/services/offline.c` |
| 5 | Jogo parado, nenhum frame | novo dump de threads (`LARECOMP_THREAD_DUMP`) | deadlock de mutex: no timeout de condvar o kernel readquiria o mutex, e o SDK (`TimedWait` → `Enter` em `0xea01`) também | kernel não readquire no timeout | `runtime/kernel.c` |
| 6 | Crash em `nvnCommandBufferBindProgram` | símbolo do runtime via `nm` | o jogo faz bind de programa NULL no início | (depois substituído pelo item 22) | `runtime/nvn/cmdbuf.c` |
| 7 | "Press a button on the controller you want to use" sem aceitar o controle | captura de frame (`LARECOMP_SHOT_AT`) + `SetSupportedNpadStyleSet 0x5a` | o runtime apresentava um **Pro Controller**, que o Let's Go não aceita | controle como **modo portátil** (configurável por `LARECOMP_NPAD`) | `runtime/hle/input.c`, `services/hid_set.c` |
| 8 | Crash ao abrir o vídeo de título | backtrace: `movie::MediaData::FindInt64` | 2 das 28 funções de vídeo sem HLE (já previsto no diagnóstico) | `FindInt64` e `SetCoreMask` | `runtime/hle/movie.c` |
| 9 | ~240 fps, áudio picotado | estatística do renderer | o runtime ignorava o *present interval* do jogo | espera pelo intervalo do jogo (2 = **30 fps**), `LARECOMP_FPS` para mudar | `runtime/nvn/objects.c` |

### O vídeo de título (o caminho mais longo)
| # | Sintoma | Diagnóstico | Causa | Correção |
|---|---|---|---|---|
| 10 | GPU "device lost" (escrita no endereço 0) | camada de validação do Vulkan | texturas do vídeo eram encolhidas pela **escala de resolução interna** e a cópia em tamanho cheio estourava | texturas preenchidas pela CPU ficam em tamanho cheio (`vk_tex_unscale`) |
| 11 | Tela verde (NV12 com Y=UV=0) | testes por eliminação: sem escala, buffers no heap do jogo, preencher com cinza → continuava verde | — (hipóteses descartadas, mas viraram melhorias: `backend_pool_written`, alocador do jogo) | ver item 12 |
| 12 | Tela verde | log de quem lê os quadros: o jogo nunca chamava `Buffer::Base` no vídeo; `GetOutputBuffer` recebia um **buffer do próprio jogo** (1920×1080×1,5) | o runtime trocava o buffer do jogo pelo seu; o jogo continuava lendo o seu, vazio | o quadro é copiado no buffer do jogo, em NV12 compacto |
| 13 | Áudio do vídeo "robótico" | gravação WAV da saída + comparação com o arquivo original (70% das amostras exatas) + teste de ouvido do usuário | o decoder reusava na hora o buffer de saída recém-liberado, enquanto o jogo ainda copiava dele | slots de saída em rodízio |
| 14 | Música e efeitos mudos (saída de 6 canais toda em zero) | gravação WAV por saída | `hwopus` (decodificador Opus) era stub: o jogo desistia | serviço `hwopus` implementado com o Opus do FFmpeg |
| 15 | Vídeo congelava em quadro aleatório | dump de threads: thread de vídeo esperando um mutex **dela mesma** | corrida no kernel: um sinal chegava junto com o timeout, o kernel entregava o mutex e mesmo assim respondia "timeout" | mantém o resultado de quem acordou a thread |
| 16 | O jogo nunca saía do vídeo (tela branca no fim) | estado dos decoders + contagem de pacotes do arquivo (3.458) | no fim do arquivo o extrator devolvia `1` (erro genérico, "tente de novo") | devolve `0x2000`, o "fim de stream" de verdade, achado na própria biblioteca do jogo (`NnMediaCommon::AndroidErrorToNnError`) |

Arquivos: `runtime/hle/movie.c`, `runtime/nvn/vulkan/textures.c`, `exec.c`, `runtime/services/hwopus.c`,
`runtime/services/audio.c`, `runtime/kernel.c`.

---

## Parte 5: introdução, 3D e gameplay

| # | Sintoma | Diagnóstico | Causa | Correção | Arquivo |
|---|---|---|---|---|---|
| 17 | Nome "RRS" aparecia como "?S" | bytes: `52 52 53` lidos como UTF-16 = `U+5252` + `S` | o teclado devolvia UTF-8; o Let's Go lê UTF-16 | resposta em UTF-16LE | `runtime/hle/swkbd.c` |
| 18 | Sem 3D (tela preta com som no quarto) | contagem de draws pulados por motivo: todos "noobj" | o jogo faz bind de **programas separados por estágio** (vertex num, fragment noutro, NULL para desligar); o runtime trocava o programa inteiro a cada bind | programa combinado por estágio (`bind_program_stages`) | `runtime/nvn/cmdbuf.c`, `vulkan/exec.c` |
| 19 | ~4.000 draws/2 s ainda pulados | log dos estágios: `0x1` | programas só com vertex shader (passes de profundidade/sombra) | fragment shader vazio sintetizado | `runtime/nvn/vulkan/exec.c` |
| 20 | Mira da Poké Ball ia para o céu | — (relato do usuário) | sensor de movimento (`SixAxisSensor`) sem implementação: o SDK lia lixo | sensor neutro (parado, nivelado, apontado para a frente) | `runtime/hle/input.c` |

Resultado: **cenário 3D correto** (quarto, Pallet Town, céu), captura do Pikachu, **save e load funcionando**
(`save/save_0/savedata.bin`).

---

## Parte 6: personagens "explodidos" (resolvido)

Os modelos com esqueleto (personagens, Pokémon) apareciam esticados em "espinhos" que convergiam para um ponto.
O cenário estático estava correto, então câmera e geometria comum funcionavam. Plano e experimentos completos em
[PLANO-3D.md](PLANO-3D.md).

| # | Sintoma | Diagnóstico | Causa | Correção | Arquivo |
|---|---|---|---|---|---|
| 21 | Personagens e Pokémon em espinhos | captura no RenderDoc + o shader rodado na CPU com os mesmos dados da GPU: saídas idênticas, e o `w` de saída proporcional ao peso do **1º** osso | `LDC.64 R4, c[4][R4+0x38]` (carrega R4 e R5 com R4 como endereço) virava `r4 = cb4[r4+56]; r5 = cb4[r4+60]`: a segunda leitura usava o R4 **já sobrescrito**. O `w` da translação do 2º osso (que deveria ser 1,0) vinha zero ou lixo, e o vértice ia para perto do plano da câmera | o tradutor copia o registrador de endereço para uma temporária quando ele também é destino | `runtime/nvn/maxwell/translate.c` |

### O caminho até a causa
1. **Descartados com dados** (antes do plano): formato de atributo, índices e pesos em memória, matrizes da paleta,
   momento da cópia dos uniforms, cópia dos vértices.
2. **Fases 0 e 1** do plano: reprodução determinística com o save (`tools/repro.sh`) e cortes binários na GPU. Peso
   100% no 1º osso deixava os modelos inteiros; o 2º osso estava errado. A conclusão da época, "o valor de `a11.y`
   chega errado na GPU", estava **errada**: o teste do shader na CPU (X5) tinha passado por coincidência, porque o
   vértice usado não exercitava a leitura defeituosa.
3. **Fase 3, RenderDoc**: os bytes de vértice que a GPU lê (incluindo `a11 = [7, 6]` e os pesos) eram idênticos aos
   da memória. Rodar o shader traduzido na CPU com os dados **exatos** da GPU (vértices e `cb3`/`cb4` exportados da
   captura) reproduziu a saída da GPU número por número. Então o defeito estava no próprio shader traduzido. O log
   das leituras de `cb4` no harness e a leitura do GLSL apontaram as duas linhas acima.

Enquanto a correção não existia, o `jogar.bat` usou um paliativo (`LARECOMP_VK_SKIN_ONEBONE=1`, peso 100% no 1º
osso: modelos inteiros, animação mais rígida).

---

## Parte 7: interface

| # | Sintoma | Diagnóstico | Causa | Correção | Arquivo |
|---|---|---|---|---|---|
| 22 | Ícones dos Pokémon "picotados" (equipe, tela de experiência após captura, Pokédex) | log das texturas do draw (`68x56`, formato `0x3B`) e decodificação da textura gravada com 4 layouts de bits: só "R nos bits baixos, alfa no bit 15" dá o ícone certo | o NVN `RGB5A1` (e o `RGBA4`) tem o vermelho nos bits baixos; o Vulkan `R5G5B5A1_UNORM_PACK16` tem o vermelho nos altos e o alfa no bit 0: cores trocadas e alfa tirado de um bit de cor | convertidos para RGBA8 no upload, como o RGB8 | `runtime/nvn/vulkan/textures.c` |

---

## Lições aprendidas
- **Medir antes de chutar.** Os maiores avanços vieram de instrumentação: dump de threads, motivo dos draws
  pulados, gravação do áudio, log de quem chama cada função.
- **O LASRecomp é "certo para o Zelda".** Vários bugs eram suposições que valiam para o Zelda e não para o Let's Go:
  formato de texto, Pro Controller, programas por estágio, buffer de vídeo do jogo, código de fim de stream.
- **A biblioteca do próprio jogo é a referência.** Ler o disassembly do SDK e do `subsdk1` (por exemplo
  `TimedWait`, `AndroidErrorToNnError`) deu a resposta exata em vez de tentativa e erro.
- **Corridas de thread aparecem como sintomas aleatórios.** O vídeo travava num quadro diferente a cada execução;
  a causa era uma corrida no kernel, não o vídeo.
- **Um teste que passa precisa exercitar o caso que falha.** O shader na CPU "provou" que a tradução estava certa
  com vértices que não passavam pela instrução defeituosa. Rodar com os dados exatos de um draw quebrado (exportados
  do RenderDoc) e comparar com a saída da GPU resolveu em minutos.
- **Tradução de registradores: cuidado com destino que também é fonte.** Instruções que escrevem vários
  registradores (`LDC.64`, `.128`) podem ter o endereço entre os destinos; leia as fontes antes de escrever.
