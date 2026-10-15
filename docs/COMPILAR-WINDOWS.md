# Compilar e jogar no Windows

Passo a passo do zero até o jogo aberto. Leva por volta de meia hora, quase toda esperando downloads e a compilação.

## O que você precisa
- Windows 10 ou 11, 64 bits
- CPU com AVX2 (Intel a partir de 2013 / Haswell, AMD a partir de 2015 / Excavator)
- GPU com Vulkan 1.3 e driver atualizado (NVIDIA, AMD ou Intel recentes)
- Uns 10 GB livres (arquivos do jogo, código gerado e build)
- [Git for Windows](https://git-scm.com/download/win) (traz o Git Bash)
- **Os seus próprios arquivos do jogo**: *Pokémon: Let's Go, Eevee!* **v1.0.0**, extraídos da sua cópia. Este
  projeto não fornece nem ensina a obter esses arquivos ou as chaves.

## 1. Baixar o projeto
No Git Bash, numa pasta com espaço livre (de preferência fora de `Arquivos de Programas`):

```bash
git clone <endereço deste repositório> letsgo-recomp
```

## 2. Instalar as ferramentas (uma vez)
No PowerShell, dentro da pasta do projeto:

```powershell
powershell -ExecutionPolicy Bypass -File tools\setup_windows.ps1
```

O script instala:
- o **MSYS2** em `C:\msys64`, com clang, make, SDL3, FFmpeg, Vulkan e shaderc;
- o **Python 3.12**, com o ambiente do gerador de código em `.venv`.

Ele também configura o clone para fim de linha LF. Pode rodar de novo se algo falhar no meio.

## 3. Colocar os arquivos do jogo
Crie a pasta `extracted/` na raiz do projeto com:

```
extracted/
  exefs/      main, main.npdm, rtld, sdk, subsdk0, subsdk1   (o ExeFS do jogo)
  romfs.bin   a imagem RomFS do jogo (uns 4,4 GB)
  romfs/      opcional: o RomFS extraído em pastas (útil para mods; o jogo lê o romfs.bin)
```

Confira se é a **v1.0.0** (sem atualização aplicada): o código gerado depende do executável exato.

`extracted/` está no `.gitignore` e nunca deve ir para o git.

## 4. Gerar e compilar
Abra o **Git Bash** na pasta do projeto e coloque o MSYS2 no PATH (vale para essa janela):

```bash
export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"
```

Traduza o código do jogo para C (uma vez; o resultado vai para `generated/`, fora do git):

```bash
make gen
```

Compile o runtime e o código do jogo. **Sempre com `-j`**: sem ele, os ~920 arquivos do jogo compilam um por vez e
passam de uma hora.

```bash
make -j20
```

Isso gera `build/lasrecomp.exe` (o runtime) e `build/lasgame.dll` (o código do jogo). A DLL leva de 5 a 20 minutos,
conforme a CPU. Depois, mudanças só no runtime recompilam em segundos (`make -j20 build/lasrecomp.exe`).

Monte a cópia jogável, com o exe, a DLL do jogo, as DLLs do MSYS2 e o ícone:

```bash
make jogo
```

## 5. Jogar
Dê dois cliques em **`jogar.bat`**. Ele abre o launcher; escolha a resolução e clique em **Jogar**.

- Os saves ficam em `save/`, as configurações em `larecomp.ini` e os caches de shader em `cache/`, tudo na pasta do
  projeto.
- O log vai para `logs\jogo.log`.
- A primeira vez em cada área compila os shaders. Pode haver objetos aparecendo atrasados por um instante; da segunda
  vez em diante fica liso.

Também dá para abrir direto do Git Bash (`--play` pula o launcher):

```bash
./build/lasrecomp.exe extracted --play
```

## Opcional
- **Tradução / mods:** coloque o mod em `mods/<nome>/` (formato do Atmosphere) e escolha-o em "Tradução" no launcher.
- **Fonte do sistema do Switch:** veja [fonts/README.md](../fonts/README.md).
- **Melhor imagem:** com uma GPU folgada, escolha uma resolução acima da sua tela (por exemplo 4K numa tela 1080p):
  o jogo é renderizado maior e reduzido, o que elimina quase todo o serrilhado.

## Problemas comuns
| Sintoma | O que fazer |
|---|---|
| "avutil-…dll / SDL3.dll não encontrado" ao abrir o exe | use o `jogar.bat` ou a cópia de `make jogo` (traz as DLLs), ou ponha `C:\msys64\ucrt64\bin` no PATH |
| "lasgame.dll was built for another version" | o exe e a DLL saíram de builds diferentes: `make -j20` e depois `make jogo` |
| `make gen` falha | confira o `.venv` (rode de novo o `setup_windows.ps1`) e se `extracted/exefs/main` existe |
| tela preta ou erro de Vulkan | atualize o driver da GPU; precisa de Vulkan 1.3 |
| o jogo fecha sozinho | mande o `logs\jogo.log` num issue (veja [CONTRIBUTING.md](../CONTRIBUTING.md)) |

## Para quem vai mexer no código
- [DEPURACAO.md](DEPURACAO.md): variáveis de diagnóstico e o `tools/repro.sh`, que roda o jogo com entrada roteirizada e
  tira prints em quadros escolhidos.
- [HISTORICO.md](HISTORICO.md): como cada problema foi resolvido. É a melhor forma de entender o runtime.
- Mudou `runtime/cpu.h` ou `runtime/gamelib.h`? A DLL do jogo precisa ser recompilada (`make -j20`).
