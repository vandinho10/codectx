# Changelog

Todas as mudanças notáveis neste projeto serão documentadas neste arquivo.

O formato segue [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/) e o projeto adere ao [Versionamento Semântico](https://semver.org/lang/pt-BR/) (`MAJOR.MINOR.PATCH`) com [Conventional Commits](https://www.conventionalcommits.org/pt-br/).

## [Unreleased]

### Alterado
- `README.md`: referências de versão atualizadas (1.0.0 → 1.3.0) e nome do
  arquivo padronizado em maiúsculas (`Readme.md` → `README.md`).

## [1.3.0] - 2026-08-22

### Adicionado
- **Configuração persistente** (estilo `git config`): subcomando `codectx config`
  com verbos `list` (padrão), `get`, `set`, `add`, `unset` e `path`, escopos
  `--global` (padrão) e `--local`.
- Dois níveis de arquivo: global em
  `$XDG_CONFIG_HOME/codectx/config` (`%APPDATA%\codectx\config.txt` no Windows)
  e local `.codectx.conf` descoberto subindo a hierarquia de diretórios a partir
  do cwd (commitável por design; oculto também no Windows via atributo hidden).
- Precedência: **CLI > local > global > defaults**. Categorias não fornecidas
  na CLI são preenchidas pela config; `-a/--add` e `-n/--not` sempre somam.
- Chaves suportadas: `alvos`, `ext`, `file`, `add`, `not`, `output`, `no-rec`.
- Gravação atômica (tmp + rename), formato `chave=valor` (uma linha por valor,
  comentários com `#`), avisos para linhas/chaves inválidas.
- Variável `CODECTX_CONFIG_HOME` para redirecionar o nível global (sandbox/testes).
- 12 novos testes (roundtrip, precedência, walk-up, ciclo set→get→unset,
  `--local` etc.).

### Alterado
- `main.cpp`: intercepta `codectx config ...`; fluxo normal aplica os níveis de
  configuração após `parse_args`.
- `parse_args`: marca categorias tocadas na CLI (`cli_*`) sem mudar semântica
  existente — todos os testes anteriores permanecem válidos.

## [1.2.0] - 2026-08-22

### Adicionado
- Suporte a **Windows**: binários estáticos `codectx-windows-amd64.exe` e
  `codectx-windows-x86.exe` via cross-compile mingw-w64 (`make
  TARGET_OS=Windows_NT`), zero dependências em runtime. Validado em máquina
  Windows real (v1.1.0: recursão, filtro por extensão e `-o` funcionais).
- Construtor multiplataforma `cross.sh` (nativo + containers docker +
  mingw), mesmo padrão do projeto `invpush`.
- CI: matriz de release estendida com os dois alvos Windows (5 artefatos).

### Alterado
- `Makefile`: variável `TARGET_OS` com linkagem estática no Windows;
  `clean` remove também o `.exe` gerado pelo ld PE.

## [1.1.0] - 2026-08-21

### Adicionado
- CI/CD: GitHub Actions com Docker + QEMU para build multi-arch (x86_64, aarch64, arm32).
- Binários compilados na release: `codectx-linux-x86_64`, `codectx-linux-aarch64`, `codectx-linux-arm32`.

### Corrigido
- `.gitignore`: permitido binário por arch (ignora apenas `codectx-linux-x86_64`).

## [1.0.0] - 2026-08-07

### Adicionado
- `--version` / `-v` exibindo a versão (SemVer) do binário.
- Arquitetura modular e testável: `codectx.hpp`/`codectx.cpp` + `main.cpp` enxuto.
- Suíte de testes table-driven (unitários + integração) em `tests/` com framework próprio.
- Pipeline de verificação no Makefile: `check` (warnings como erro), `test`, `sanitize` (ASan/UBSan), `version`, `install`.

### Corrigido
- Crash por injeção de regex no filtro `-e` (entrada do usuário agora é escapada antes de compor o padrão).
- `-e` engolia o alvo seguinte (`codectx -e cpp pasta/`): caminhos existentes ou com parent path são tratados como alvos, não como extensão.
- Arquivos binários (`*.bin`, `*.o`, `*.obj`, `*.a`, `*.lib`, `*.wasm`, `*.iso`, `*.img`, `*.dat`, `*.pkl`, `*.npy`, `*.h5`, `*.pb`, `*.parquet`) agora são ignorados na varredura padrão.
- `-h` não chama mais `exit(0)` dentro do parser (RAII/flush preservados).
- Falhas de sistema de arquivos durante a varredura são capturadas (`filesystem_error`), evitando término abrupto do processo.
- Código de saída: erro ao abrir o arquivo de saída agora retorna `EXIT_FAILURE`.

### Interno
- Uso de `std::filesystem::filesystem_error` (tipo padrão do GCC/libstdc++) nos `catch`.
- Removido parâmetro morto `is_dir` de `deve_ignorar` (build 100% limpo com `-Werror`).
- `fs::file_size` com sobrecarga `std::error_code` (sem exceção).
