# Changelog

Todas as mudanças notáveis neste projeto serão documentadas neste arquivo.

O formato segue [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/) e o projeto adere ao [Versionamento Semântico](https://semver.org/lang/pt-BR/) (`MAJOR.MINOR.PATCH`) com [Conventional Commits](https://www.conventionalcommits.org/pt-br/).

## [Unreleased]

### Adicionado
- `LICENSE` (MIT) + `NOTICE` na raiz do repositório (licenciamento do projeto).

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
