# Plano: corrigir os modelos 3D com esqueleto

_Criado em 15/10/2026. Contexto completo em [HISTORICO.md](HISTORICO.md) (parte 6) e ferramentas em
[DEPURACAO.md](DEPURACAO.md)._

## O problema
Personagens e Pokémon (modelos com esqueleto) aparecem esticados em "espinhos" que **convergem para um ponto no
próprio personagem** (perto dos pés/centro do modelo). Partes reconhecíveis do modelo (olhos, boné) aparecem no
lugar certo. O **cenário estático é renderizado corretamente** (quarto, Pallet Town, céu), então câmera, iluminação,
texturas e geometria comum funcionam.

## Evidências

### O que já está comprovado
| # | Evidência | Como foi obtida | O que descarta |
|---|---|---|---|
| E1 | Cenário estático correto em várias cenas | prints do usuário (quarto, Pallet Town) | erro geral de câmera, projeção, profundidade ou formato de vértice comum |
| E2 | Nenhum formato de atributo desconhecido | log em `vertex_format` | atributo lido com formato errado |
| E3 | Layout dos vértices com esqueleto: posição RGB32F, normal RGBA16F, UV RG32F, **a11 = índices RGBA8 UINT**, **a12 = pesos RGBA8 UNORM**, stride 44/48/56 | `LARECOMP_DEBUG_FRAME` / `LARECOMP_LOG_SKIN` | — |
| E4 | Vértices com valores coerentes: ossos `25, 24`, pesos `238 + 17 = 255` (100%) | dump dos primeiros vértices | dados de vértice corrompidos |
| E5 | Paleta de ossos (`cb4`) válida: 131 ou 107 matrizes 4x4, rotações + translações, animando a cada frame, sem NaN | estatística da paleta | matrizes de osso calculadas errado pela CPU |
| E6 | Paletas sempre com tamanho válido (8.384 / 6.848 bytes) | log de `vcb` | buffer vinculado sem tamanho |
| E7 | O shader de skinning lê o índice de `a11` (declarado `uvec4`, correto), multiplica por 64 e lê 4x4 em `cb4` com `LDC.64` | leitura do GLSL traduzido | declaração de tipo errada do índice |
| E8 | As instruções `LDC.64` conferidas estão traduzidas certo (offset, buffer, tamanho) | decodificação manual | — (só para as instruções vistas) |
| E9 | O shader usa `cb3` linhas 4–7 (matriz do mundo) e **8–11 (câmera, view-projection)** para o `gl_Position` | leitura do GLSL | — |
| E10 | Programas por estágio e só com vertex shader já são aceitos; 0 draws pulados por "noobj" | `[skip: ...]` | draws de personagem sendo pulados |

### O que é suspeito, mas não comprovado
| # | Observação | Por que é suspeita |
|---|---|---|
| S1 | No **único** dump feito (o 1º draw com esqueleto, ainda na introdução), `cb3` linhas 8–11 (câmera) e 0–3 estavam **zeradas** | com câmera zerada o `gl_Position` sai zero, como o harness mostrou. Mas foi cedo demais: pode ter sido um frame sem câmera configurada |
| S2 | Cenas 3D têm ~360 compute dispatches a cada 2 s | o jogo pode calcular dados (matrizes, vértices) na GPU; o renderer faz cópias da CPU antes de a GPU rodar |
| S3 | O ponto de convergência fica **no personagem** | vértices com todos os ossos zerados caem na origem do modelo, que o mundo leva até a posição do personagem. Indica ossos lidos como zero na GPU, apesar de a paleta estar certa na memória |

### Experimentos inconclusivos (refazer com verificação)
| Experimento | Resultado | Por que é inconclusivo |
|---|---|---|
| `LARECOMP_VK_UBO_DIRECT=1` (uniforms direto da memória) | sem mudança | sem contador: não se sabe se a condição de alinhamento deixou ele valer para `cb3`/`cb4` |
| `LARECOMP_VK_COPY_VERTICES=1` | sem mudança | idem, mas E4 já mostra que os vértices estão certos |
| Harness na CPU | `gl_Position = 0` | rodou com o dump S1 (câmera zerada); precisa de um dump de gameplay |

## Hipóteses (da mais provável para a menos)
| H | Hipótese | Explica | Teste decisivo |
|---|---|---|---|
| H1 | Na GPU, os ossos chegam **zerados ou parciais** (o vínculo de `cb4` não cobre a paleta, ou é lido de outro lugar/momento) | S3 (convergência no personagem), E5 (memória certa) | X2 + X4 |
| H2 | A **câmera (`cb3` 8–11) chega zerada ou desatualizada** na GPU (preenchida por compute, `UpdateUniformBuffer` ou depois da cópia) | S1, S2 | X1 + X3 |
| H3 | **Erro de tradução** em alguma instrução do caminho de skinning (soma ponderada, conversão de peso, etc.) | partes certas e partes erradas no mesmo modelo | X5 |
| H4 | Os ossos vêm de **outra fonte** em alguns shaders (há variantes: `cb4`, `cb9`, um shader por `gl_VertexIndex`) | modelos diferentes podem falhar de jeitos diferentes | X6 |

## Experimentos (em ordem de custo)

### Fase 0: reprodução determinística (pré-requisito)
- [ ] Script de teste que carrega o **save** e entra no gameplay sozinho (`LARECOMP_INPUT` com A na tela de título),
      captura frames fixos e grava o log. Meta: a mesma cena, sempre, em ~1 minuto.
- [ ] Fixar a captura por **frame**, e não pela ordem do draw (o dump do "n-ésimo draw" mudava de cena entre execuções).

### Fase 1: cortes binários baratos (cada um responde sim/não)
- [ ] **X1. Câmera no gameplay:** dump do `cb3` de um personagem no quarto. Linhas 8–11 preenchidas? Quem escreve?
      (log de `UpdateUniformBuffer` no endereço e checagem se algum compute escreve ali).
- [ ] **X2. Paleta identidade:** na cópia do `cb4`, trocar todas as matrizes por identidade. Se os personagens
      aparecerem certos em pose padrão (T-pose), o caminho mundo/câmera está certo e o problema é a paleta na GPU (H1).
      Se continuarem explodidos, o problema está em `cb3`/câmera (H2) ou na tradução (H3).
- [ ] **X3. Refazer `UBO_DIRECT` com contador:** contar quantos binds de uniform viraram leitura direta. Se vale para
      `cb3`/`cb4` e o resultado muda, os dados são escritos pela GPU depois da cópia (H2/S2).
- [ ] **X4. Um osso só:** injetar no GLSL peso 100% no primeiro osso (`#define` sobre `a12`). Se o modelo ficar
      inteiro (rígido, mas no lugar), o problema está nos ossos secundários ou na soma ponderada.

### Fase 2: harness na CPU com dados de gameplay
- [ ] **X5.** `LARECOMP_DUMP_SKIN` + `make_harness.py` num draw de personagem do gameplay. Comparar o `gl_Position`
      do shader traduzido com a referência (skinning + `cb3` 4–7 + 8–11):
      - **iguais e coerentes:** a tradução está certa; o erro está no caminho de dados até a GPU (H1/H2).
      - **diferentes:** erro de tradução (H3). Bisseção: imprimir registradores intermediários e achar a primeira
        instrução que diverge da referência.

### Fase 3: ver o que a GPU realmente recebe
- [ ] **RenderDoc** (gratuito): capturar um frame e, no draw do personagem, ver os uniforms como a GPU os vê e o
      *mesh viewer* com a saída do vertex shader por vértice. Responde H1/H2 de forma direta.
- [ ] **X6.** Listar todas as variantes de shader com esqueleto (`cb4`, `cb9`, por índice de vértice) e repetir
      X5 em uma de cada.

### Fase 4: corrigir e travar
- [ ] Aplicar a correção conforme a causa:
      - **H1/H2 (dados):** ler esses uniforms direto da memória na hora certa, ou sincronizar a cópia com o compute.
      - **H3 (tradução):** corrigir a instrução em `runtime/nvn/maxwell/translate.c` e criar um teste
        (instrução isolada no harness).
- [ ] Regressão visual: três cenas fixas (quarto, Pallet Town, batalha) com capturas de referência, comparadas a cada
      mudança no renderer (ideia do modo determinístico do ssbu-recomp).
- [ ] Registrar no [HISTORICO.md](HISTORICO.md) e atualizar o [PROGRESSO.md](PROGRESSO.md).

## Plano B (se a camada NVN não der conta)
O HWDER recompila o **próprio driver NVN** do jogo (que já está no nosso `sdk` traduzido) e implementa um
**processador de comandos Maxwell**. Nesse modelo não existem suposições sobre a API NVN, só sobre a GPU. É uma
mudança grande de arquitetura: só vale se as fases 1–3 mostrarem vários problemas de API que não dá para corrigir
um a um. Até lá, fica só como referência (licença do HWDER não declarada: estudar, não copiar).

## Referências
| Fonte | Para quê |
|---|---|
| `runtime/nvn/vulkan/exec.c` (`cbuf_info`, `draw_`, `bind_program_stages`) | onde uniforms, vértices e programas chegam ao Vulkan |
| `runtime/nvn/maxwell/translate.c` | tradutor Maxwell → GLSL |
| `tools/skintest/` | harness de shader na CPU |
| [HWDER](https://github.com/sonsegajp/HWDER): `runtime/src/shader/`, `runtime/src/gpu/gpu.cpp`, `texture_cache.cpp` | plano B; tradutor SASS → SPIR-V para comparar semântica de instruções |
| [ssbu-recomp](https://github.com/Denkishi/ssbu-recomp): `runtime/mx_translate.c`, `nvn_vk*.c`, `tests/DEBUG_MODE.md` | mesma camada que a nossa (NVN → Vulkan); modo determinístico com hash por frame |
| RenderDoc | inspeção do que a GPU recebe em cada draw |
| Switchbrew / envytools | referência das instruções Maxwell e da NVN |

## Critério de pronto
Personagens e Pokémon renderizados corretamente no quarto, em Pallet Town e numa batalha, sem regressão no cenário,
no vídeo e na interface, e o problema documentado no histórico.
