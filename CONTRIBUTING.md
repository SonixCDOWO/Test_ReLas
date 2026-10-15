# Contribuindo

Relatos de problemas, testes em outros PCs e código são bem-vindos.

> [!NOTE]
> O projeto é desenvolvido principalmente com IA (veja o README). Contribuições escritas à mão, com ajuda de IA ou
> qualquer coisa no meio valem igualmente: o que importa é que a mudança funcione, tenha sido testada e siga as regras
> abaixo.

## A regra que não muda: nada da Nintendo

Nunca podem entrar no repositório nem em issues ou pull requests:

- cópias do jogo em qualquer formato (`.xci`, `.nsp`, `.nca`…), chaves (`prod.keys`, `title.keys`)
- arquivos extraídos (`extracted/`), código gerado (`generated/`), fontes do sistema do Switch (`.bfttf`)
- qualquer asset do jogo: texturas, modelos, música, textos, arquivos do romfs, nem "só um arquivo para testar"
- saves (`save/`) e mods com arquivos do jogo (`mods/`)

Prints e vídeos curtos do jogo em relatos de problema são bem-vindos.

## Reportando problemas

Abra um issue com:

- o seu PC: CPU, GPU, versão do driver e tamanho da janela ou resolução escolhida
- o que você fez, o que aconteceu e o que esperava
- um print ou vídeo curto, se for visual
- se o jogo fechou ou travou: o `logs\jogo.log` (o `jogar.bat` grava ali)
- se usava tradução ou mod, qual

## Código

- Compile e teste como em [docs/COMPILAR-WINDOWS.md](docs/COMPILAR-WINDOWS.md).
- Cada correção ganha uma linha em [docs/HISTORICO.md](docs/HISTORICO.md) (sintoma, diagnóstico, causa, correção), e
  [docs/PROGRESSO.md](docs/PROGRESSO.md) acompanha o estado.
- Variáveis e ferramentas de teste novas vão para [docs/DEPURACAO.md](docs/DEPURACAO.md).
- O runtime veio do LAS Recomp, feito para o Zelda. Antes de culpar o jogo, confira se uma suposição do runtime vale
  para o Let's Go (formatos, estilos de controle, códigos de retorno, resolução informada).
- Para mudar o comportamento do jogo sem gerar a DLL de novo, use os ganchos de função (`runtime/fnhook.c`).
