# Modo de jogo: batalhar com Pokémon selvagens

_Ideia (15/10/2026): ao encostar num Pokémon selvagem, em vez de ir direto para a captura (estilo Pokémon GO), entrar
numa batalha clássica - e capturar depois, como o jogo já faz com os lendários e o Snorlax. Até onde se sabe ninguém
fez isso em mod do Let's Go ([discussão de 2026](https://reddit.sentinel-team.org/posts/1rr2xif/snapshots/2026-03-13T05%3A19%3A13.96841Z))._

## Etapa 1: mapa dos dois caminhos ✅

Método: `LARECOMP_FNPROF` (runtime/fnprof.c: funções do jogo executadas numa janela de frames, agora com a ordem da
primeira entrada e o chamador) em rodadas sem encontro, com encontro selvagem e com batalha de treinador (save na
Rota 1, ao lado do Youngster Ronny); `tools/fncompare.py` separa o que é comum e o que é de cada evento, e
`LARECOMP_FNPROF_BT=<addr>` + `LARECOMP_FNPROF_BT_MAX=<n>` mostram a cadeia de chamadas.

Atualização do mapa (todo quadro): `main+13c8f0` → `13d2f0` → `315390` → `2f78d0` → **`2f8bb0`**, que chama:
- **`2fda00`** (Pokémon selvagens no mapa). No encostar: → **`305ad0`** → **`28ef70`**`(x0 = objeto do encontro,
  x1 = gerenciador global [*0x70015faab8]+0x68)`, que aloca a cena de captura (0x3B0 bytes) e a empurra:
  `31c030` → `139e80` (empilhar processo/cena).
- **`2facf0`** / `2fa860` (visão dos treinadores). Ao ver o jogador: `17a7f0` → `17aa70` → `17b380` → `139e80`
  (evento: "!" e diálogo).

Batalha de treinador, depois do diálogo: interpretador de script `17b6c0` → `19ca10` → `19b7f0` → comando
`674ed0` → `672484` → `66fc20` → `17e150` → **`28bb60`**`(x0 = &objeto, x1, x2 = &parâmetros)`, que copia uma
configuração de batalha de **0x30 bytes** para o objeto da batalha (+0x88) e troca de cena (`28bc30` → `28e480` →
`139e80`); dentro da batalha, `28be80` → `28ccb0` → `28b150` → `31a470`.

Conclusão: captura (`28ef70`) e batalha (`28bb60`/`28e480`) são métodos vizinhos do mesmo gerenciador de encontros
(`main+28xxxx`). O desvio seria em `305ad0`: montar uma configuração de batalha selvagem para o Pokémon tocado e
chamar o início de batalha no lugar da captura; depois da batalha, iniciar a captura (o fluxo dos lendários).

## Etapa 2: ganchos e primeiros dados ✅

- **Ganchos de função** (runtime/fnhook.c, `FN_ENTER` em runtime/cpu.h, ABI da DLL 3): o runtime pode tratar ou
  envolver qualquer função do jogo sem gerar a DLL de novo. `LARECOMP_HOOK_DUMP` registra argumentos e memória.
- **Batalha do Youngster Ronny** (`28bb60`, chamado só pelo comando de script `17e150`):
  - `x2` = parâmetros de 0x30 bytes: `{0, 0, 0x1C1, 0, 0x200, ...}`; o comando de script recebe os mesmos 0x1C1 e
    0x200 - **0x1C1 (449) deve ser o treinador**, 0x200 flags;
  - `*x0` = objeto grande com contagem de referências (vtable `0x7001537770`): o pedido de batalha do evento.
- **Outros caminhos** (referências diretas no `main`, `tools/xrefs.py`):
  - `28bc30` (inicia a batalha de fato) tem dois chamadores: `28bb60` (script) e **`28bda0`**, chamado por
    `289c4c` - um método virtual que monta os parâmetros com um **modo 0 ou 3** conforme a flag `[obj+0xA6]`;
  - a captura `28ef70` tem quatro chamadores: o encontro selvagem (`305ac4`), `28ea90`, `854a48` e **`31a0e0`**,
    chamado pelo método virtual `319e20`, ao lado da sequência de batalha (`31a470`) - candidato a "batalha e depois
    captura" dos lendários.

## Próximas etapas
3. Confirmar o caminho alternativo (`289c4c` → `28bda0`, modo 3) e a captura pós-batalha (`319e20` → `31a0e0`) em
   execução: o ideal é um save perto de uma batalha selvagem nativa (lendário ou Snorlax); sem isso, forçar o
   método e observar.
4. **Protótipo**: no encontro selvagem (`305ad0`), em vez de `28ef70`, montar o pedido de batalha selvagem para o
   Pokémon tocado e iniciar a batalha; no fim, a captura.
5. Detalhes: fugir, experiência, o Pokémon sumir do mapa, ligar/desligar o modo no launcher.
