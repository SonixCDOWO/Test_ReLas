# Pesquisa: onde a GPU gasta o quadro do mundo

_17/10/2026. Eevee 1.0.0, RTX 3080, rota da Rota 1 (`build/audit-save-20261017`, mundo parado, ~600 draws por quadro),
resolução interna 1920x1080. É pesquisa, não correção: o que está medido, o que foi descartado e o que continua sem
explicação._

## Aviso sobre o método

**O tempo de GPU do mesmo quadro varia de ~10 a ~18 ms de uma execução para outra com o mesmo código.** Comparar duas
execuções separadas só serve para efeitos grandes (2x ou mais). Várias conclusões iniciais desta pesquisa ("pilhas em
vetor: -11%", "achatar a máquina de estados: -12%") eram ruído e foram desmentidas por testes intercalados. Para efeitos
menores, alterne as versões **dentro da mesma execução** (`LARECOMP_EXP_ALT=1`: versão alterada só nos quadros ímpares,
perfil de 70 quadros seguidos, comparar as medianas de pares e ímpares) ou intercale várias execuções A/B e use só quadros
de mundo (cabeçalho do `[gpuprof]` com 6 ms ou mais). Ferramentas: [DEPURACAO.md](DEPURACAO.md).

## O que está medido

1. **O quadro de GPU é uma passada só.** `LARECOMP_GPUPROF_AT` num quadro do mundo: passe principal HDR 1920x1080 com 316
   draws = 11,9 de 14,3 ms (84%); mapa de sombras 2048x2048 = 1,6 ms (230 draws); toda a cadeia de bloom e o pós-processamento
   juntos, menos de 1 ms.
2. **Um grupo de 4 draws por quadro custa ~4,9 ms (cerca de um terço da GPU).** É o mesmo par de shaders (vertex program
   `72da7f8a00`, fragment shader `013794c6ed394fbe`), cada draw com 1.404 índices x 180 instâncias. Medido na mesma
   execução, com quadros pares (normal) e ímpares (draws pulados), 35 quadros de cada: mediana 14,58 ms contra 9,70 ms.
3. **O custo é todo de vértice.** Com scissor de 1x1 (sem trabalho de pixel, mesmo trabalho de vértice) a diferença é de
   -0,27 ms (nenhuma). Com metade das instâncias a economia é de 1,8 ms, menos que a metade do custo do grupo.
4. **Esses draws quase não produzem imagem.** `LARECOMP_GPUPROF_STATS`: 252.720 vértices de entrada e ~71.176 invocações de
   vertex shader por draw, mas 0 fragmentos e 0 primitivas no recorte em 5 de 6 amostras (917 fragmentos em uma). São ~71 mil
   vértices processados por ~0,8-1,3 ms cada draw (~15 ns por invocação).
5. **O custo está em executar o vertex shader.** Com o shader retornando no início de `main` (`LARECOMP_EXP_VS_EARLY`),
   o mesmo draw cai de ~1,3 ms para ~0,18 ms (comparação entre execuções, mas o efeito é de 7x).

## O que foi descartado (e como)

| Hipótese | Teste | Resultado |
|---|---|---|
| Filtro anisotrópico 16x | `LARECOMP_ANISO=1` | custo por índice do draw igual (4,8 contra 4,5 ns), entre execuções |
| Preenchimento de pixels / resolução | scissor de 1x1 na mesma execução | sem efeito (-0,27 ms): o custo não é de pixel |
| Leitura de vértices pelo PCIe | log do anel de memória transiente | os chunks estão em memória local da GPU (tipo `device-local host-visible`, heap de 214 MB), não em memória do sistema |
| Pilhas `ssy/pbk/pcnt` em memória local | `uvec4` no lugar de `uint[16]`, A/B intercalado em 3 rodadas | mediana 12,39 ms (arrays) contra 12,88 ms (vec4): nenhuma diferença; alteração revertida |
| Máquina de estados `for + switch (pc)` | o shader convertido à mão em blocos guardados sem laço | 0,73 contra 0,83 ms por draw, entre execuções: dentro do ruído |
| `robustBufferAccess` | `LARECOMP_NO_ROBUST=1`, 2 rodadas cada | inconclusivo (10,5 e 14,3 ms com; 14,3 e 11,0 ms sem): é a deriva entre execuções |
| ALU do shader | leitura do GLSL: 427 instruções, 98 `fma`, 3 `sin` e 3 `cos`, sem laço, sem memória local, sem acesso divergente a constantes | o custo estimado de ALU é centenas de vezes menor que o medido |

## O que continua sem explicação

Por que ~71 mil invocações de um vertex shader de 427 instruções levam ~1 ms. Ideias que ainda não foram testadas:
estatísticas do shader compilado (`VK_KHR_pipeline_executable_properties`: registradores, spills, tamanho do código); um
shader mínimo que só leia os atributos, para separar busca de atributos de cálculo; o mesmo shader num Vulkan de outro
fabricante.

## A cadeia entre as threads do jogo

- Só **3 threads trabalham** (ids 80, 81, 82, prioridade 44) e cada uma usa ~40% de CPU (total ~1,15 núcleo); as outras 7
  (83 a 89) ficam 100% paradas numa condvar (pool de workers sem uso nesta cena). Medido com `LARECOMP_THREAD_DUMP=16,0.25`
  (24 instantâneos) e CPU por thread.
- Quando "rodam", estão em `nvnQueueSubmitCommands` ou `nvnQueuePresentTexture` (`LARECOMP_HLE_WHO` dá os nomes): o backend
  Vulkan executa a lista de comandos **dentro da chamada, na thread do jogo** (3 ms de CPU por quadro) e o present espera a
  GPU e o pacing.
- Os mutexes mais disputados (`LARECOMP_PROF_LOCKS`): `72c02f5de0` (~120 mil esperas por 2 s, dono ainda não identificado) e
  três pares mutex/condvar entre essas threads (`729fae46a8`, `729fae4ae8`, `729fae4f28`).
- Sem limite de fps, o ciclo é de 12-13 ms contra ~10 ms de GPU por quadro (medido): a GPU é o maior componente, e sobram
  ~2-3 ms de CPU que não se sobrepõem a ela.

## Caminhos possíveis (nenhum implementado)

1. **Achar a causa do custo do vertex shader** (ideias acima). Se for geral, ajuda todos os draws, não só esse grupo.
2. **Não desenhar o que não aparece**: pular, com teste periódico, draws cujas estatísticas de pipeline (`fsinv`) deram 0 nos
   últimos quadros. Economia estimada de até ~4,9 ms por quadro nesta cena, com o risco de a vegetação aparecer alguns
   quadros atrasada ao virar a câmera; só como opção experimental.
3. **Tirar `backend_execute` da thread do jogo** (uma thread de renderização com fila): no máximo os ~2-3 ms de CPU que não
   se sobrepõem hoje.
