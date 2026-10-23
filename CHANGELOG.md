# Changelog

Todas as mudanças notáveis neste projeto serão documentadas neste arquivo.

O formato segue [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/) e o projeto adere ao [Versionamento Semântico](https://semver.org/lang/pt-BR/) (`MAJOR.MINOR.PATCH`) com [Conventional Commits](https://www.conventionalcommits.org/pt-br/).

## [Não lançado]

### Documentação
- `docs/README.md` reescrito para a v1.4.1: todas as opções da CLI,
  subcomandos `config`/`update`, códigos de saída, caminhos de configuração,
  destinos de instalação e fluxo de release.
- README: novas funcionalidades (bloqueio de segredos, detecção de binário,
  saída reproduzível, autoatualização) e contagem de testes atualizada.

## [1.4.1] - 2026-10-23

### Adicionado
- **Instalador** `scripts/install.sh` (Linux) e `scripts/install.ps1`
  (Windows): instala na pasta do usuário (`~/.local/bin` /
  `%LOCALAPPDATA%\codectx`), sem sudo/admin, adiciona ao `PATH` e confere o
  `SHA256SUMS`. Se o `codectx` já estiver instalado, atualiza no mesmo caminho.
- `make uninstall`.

### Alterado
- `make install` instala em `~/.local/bin` por padrão (sem sudo), para o
  `codectx update` conseguir se substituir. `PREFIX`/`BINDIR`/`DESTDIR`
  permitem outro destino (ex.: `sudo make install PREFIX=/usr/local`).
- `scripts/upgrade-to-1.4.sh` / `.ps1` viram atalhos para os novos scripts
  (as URLs antigas continuam funcionando).
- `codectx update check` retorna **2** quando há versão nova (0 = atualizado,
  1 = erro), para uso em scripts.

### Corrigido
- **`codectx update`** não encontrava nenhum asset na release: o parse do JSON
  procurava `browser_download_url` no máximo 800 caracteres após `"name"`, mas
  o objeto `uploader` deixa essa distância em ~1400. Agora o nome vem do
  basename de `browser_download_url`. Quem está na v1.4.0 precisa atualizar
  uma vez pelos scripts em `scripts/` (ou baixando o binário manualmente).
- **Windows**: comandos `curl`/`certutil` usavam aspas simples, que o
  `cmd.exe` não reconhece (User-Agent com espaços e caminho de `-o`
  quebravam). Aspas agora seguem o shell da plataforma.
- `upgrade-to-1.4.sh`: em `curl … | bash` a confirmação lia o próprio script
  e cancelava sozinha; agora lê de `/dev/tty` (ou pede `CODECTX_YES=1`).
- `upgrade-to-1.4.ps1`: não fecha mais a janela do PowerShell sob
  `irm | iex`; verificação de SHA256 funcionava só no PowerShell 7 (no 5.1 o
  conteúdo vinha como bytes e a checagem era pulada); força TLS 1.2; prompt
  mostrava caminho vazio (`$bin?` era lido como variável) e prosseguia sem
  confirmação quando não havia entrada.

## [1.4.0] - 2026-10-20

### Adicionado
- **`codectx update`**: consulta a última release no GitHub e atualiza o binário
  local (`update check`, `update`, `update --yes`).
- Requer `curl` no PATH; detecta artefato por OS/arch
  (`codectx-linux-x86_64|aarch64|arm32`, `codectx-windows-amd64|x86.exe`).
- Verifica `SHA256SUMS` quando o asset existe na release.
- CI: gera e anexa `SHA256SUMS` na release; `fail-fast: false` na matrix.

### Interno
- Novo módulo `update.cpp`; helpers de SemVer testáveis na suíte.

## [1.3.2] - 2026-10-20

### Corrigido
- Alvos explícitos (arquivo na CLI ou via `-a`) ignoram filtros `-e`/`-f` e
  só passam por checagem de binário/segredo.
- Diretório citado explicitamente (ex.: `codectx build`) não é mais silenciado
  pela blacklist de nomes.
- Detecção de binário por byte nulo nos primeiros 8 KiB (além da blacklist de
  extensão), evitando dump de executáveis sem extensão.
- Segredos bloqueados por padrão: `.env*`, `*.pem`/`*.key`, `id_rsa*`,
  `.npmrc`, `.pypirc`, `credentials.json`, etc.
- Newline final do conteúdo garantida mesmo em `stdout` (antes `tellp()`
  retornava -1 e colava o delimitador de FIM na última linha).
- Iteração de diretório usa `error_code` e continua após falhas pontuais
  (symlink quebrado / permissão), em vez de abortar o alvo inteiro.
- Deduplicação por caminho canônico (`codectx . -a src/x.cpp` não imprime 2x).
- Saída ordenada por caminho (reproduzível).
- Caminhos relativos ao cwd na saída (em vez de absolutos).
- `-o` grava via temp+rename (atômico) e ignora o próprio arquivo de saída
  por equivalência de caminho.
- Suporte a `--` para alvos que começam com `-`.
- Aviso quando `-o` vem sem argumento.
- Teste de binário: literal `"\x00..."` era truncado; usa `std::string(..., 3)`.

### Alterado
- Ajuda menciona `config`, precedência e `--`.
- `std::ios::sync_with_stdio(false)` no `main` para saída mais rápida.
- Blacklist ampliada: caches (`.pytest_cache`, `.mypy_cache`, …), lockfiles
  (`Cargo.lock`, `go.sum`, …).

## [1.3.1] - 2026-09-02

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
