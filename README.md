# Pokémon Let's Go Eevee — port nativo para PC

Port nativo para Windows do **Pokémon: Let's Go, Eevee!** (Nintendo Switch, v1.0.0), feito por **recompilação
estática**: o código ARM64 do jogo vira C e é compilado como um programa Windows. Um runtime substitui o sistema do
Switch (kernel, serviços, gráficos NVN → Vulkan, áudio, controles). Não é um emulador.

Baseado no runtime do [LASRecomp](https://github.com/peiikai/LASRecomp) (Link's Awakening, licença MIT), sem a camada
específica do Zelda. Veja [LICENSE-LASRecomp](LICENSE-LASRecomp).

## Estado
Jogável do começo: abertura, introdução, captura do Pikachu, salvar e carregar, a 30 fps. **Os modelos com esqueleto
(personagens e Pokémon) ainda aparecem distorcidos.** Detalhes em [docs/PROGRESSO.md](docs/PROGRESSO.md).

## Documentação
- [docs/PROGRESSO.md](docs/PROGRESSO.md): o que funciona e o que falta
- [docs/HISTORICO.md](docs/HISTORICO.md): cada problema encontrado, a causa e a correção
- [docs/DEPURACAO.md](docs/DEPURACAO.md): ferramentas e variáveis de diagnóstico
- [docs/DIAGNOSTICO.md](docs/DIAGNOSTICO.md): módulos do jogo e cobertura do tradutor
- [docs/ANALISE-LASRecomp.md](docs/ANALISE-LASRecomp.md): análise da base

## Requisitos
- Windows 10/11 x64, CPU com AVX2, GPU com Vulkan 1.3
- MSYS2 UCRT64 com clang, lld, make, SDL3, FFmpeg, Vulkan, shaderc
- Python 3.12 com `capstone`, `unicorn`, `lz4` (em `.venv`)
- **Os seus próprios arquivos do jogo**, extraídos do seu cartucho com as chaves do seu console

## Arquivos do jogo
Coloque em `extracted/` (fora do git):
```
extracted/exefs/   main, main.npdm, rtld, sdk, subsdk0, subsdk1
extracted/romfs/   assets do jogo
extracted/romfs.bin  imagem RomFS (hactool --romfs=...)
```

## Compilar (Git Bash)
```
export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"
make gen                    # traduz o código do jogo para generated/ (a partir de extracted/exefs)
make build/lasgame.dll      # compila o código do jogo (~22 min)
make build/lasrecomp.exe    # compila o runtime
```

## Jogar
Dê dois cliques em `jogar.bat` (o log vai para `logs\jogo.log`), ou no Git Bash:
```
PATH="/c/msys64/ucrt64/bin:$PATH" ./build/lasrecomp.exe extracted --play
```

## Regras do repositório
Nenhum arquivo do jogo, chave, código gerado a partir do jogo, save ou log entra no git (ver `.gitignore`). As chaves
precisam vir do próprio console (Lockpick_RCM); chaves baixadas não são usadas.
