# Documentação Técnica — codectx

> Referência da v1.4.1. Guia de instalação e exemplos de uso: [`README.md`](../README.md).

## Entradas (CLI)

`codectx [opções] [alvos...]` — sem alvos, varre o diretório atual (`.`).

| Opção | Descrição |
|---|---|
| `-e, --ext [EXTS...]` | Filtra por extensão (ex.: `-e cpp hpp`) |
| `-f, --file [NOMES...]` | Filtra por nome exato de arquivo (ex.: `-f Makefile Dockerfile`) |
| `-a, --add [ALVOS...]` | Adiciona arquivos/pastas preservando a base (`.`) |
| `-n, --not [NOMES...]` | Ignora arquivos ou pastas específicas |
| `-t, --target [ALVOS]` | Volta a ler os argumentos seguintes como alvos principais |
| `-o, --output [ARQ]` | Grava a saída em arquivo (temp + rename, atômico) |
| `--no-rec` | Não desce em subdiretórios |
| `--` | Fim das opções; o restante é tratado como alvo literal |
| `-h, --help` | Ajuda |
| `-v, --version` | Versão SemVer do binário |

Opções multi-argumento (`-e`, `-f`, `-a`, `-n`) consomem argumentos até a
próxima flag; um caminho existente também volta a ser lido como alvo.

### Subcomandos

| Comando | Descrição |
|---|---|
| `codectx config list\|get\|set\|add\|unset\|path [--global\|--local]` | Configuração persistente |
| `codectx update check` | Consulta a última release |
| `codectx update [--yes]` | Baixa, confere `SHA256SUMS` e substitui o binário em execução |

## Comportamento

- Varredura recursiva com filtros de inclusão (`-e`, `-f`) e exclusão (`-n`).
- Ignora por padrão: `.git`, `node_modules`, `venv`, `vendor`, `dist`,
  `build`, `__pycache__` etc., binários por extensão e por byte nulo nos
  primeiros 8 KiB, mídias e arquivos minificados.
- Segredos bloqueados por padrão: `.env*`, `*.pem`/`*.key`, `id_rsa*`,
  `.npmrc`, `.pypirc`, `credentials.json` etc.
- Alvos explícitos (na CLI ou via `-a`) ignoram `-e`/`-f`, mas continuam
  passando pelas checagens de binário e segredo.
- Saída ordenada por caminho, deduplicada por caminho canônico e com caminhos
  relativos ao diretório atual.
- Falhas pontuais de leitura (symlink quebrado, permissão) não abortam a
  varredura.

### Configuração

Precedência: linha de comando > `.codectx.conf` local (procurado do cwd para
cima) > config global > defaults. `-a` e `-n` sempre somam.

| Plataforma | Config global |
|---|---|
| Linux | `$XDG_CONFIG_HOME/codectx/config` (padrão `~/.config/codectx/config`) |
| Windows | `%APPDATA%\codectx\config.txt` |

`CODECTX_CONFIG_HOME` substitui a base dos dois casos (útil em testes).

### Atualização

- Fonte: `https://github.com/<repo>/releases/latest` (padrão
  `vandinho10/codectx`; override com `CODECTX_UPDATE_REPO=owner/repo`).
- Requer `curl` no `PATH` (nativo no Windows 10+).
- Artefato escolhido por OS/arquitetura: `codectx-linux-x86_64|aarch64|arm32`,
  `codectx-windows-amd64|x86.exe`.
- O binário antigo fica como `.old` ao lado do novo.

## Saídas

Texto no stdout (ou no arquivo de `-o`), um bloco por arquivo:

```
<><>========================================<><>
INICIO ARQUIVO: caminho/relativo.ext
========================================
...conteúdo...
========================================
    FIM ARQUIVO: caminho/relativo.ext
<><>========================================<><>
```

### Códigos de saída

| Comando | Código |
|---|---|
| varredura / `config` | `0` sucesso, `1` erro |
| `update check` | `0` atualizado, `2` há versão nova, `1` erro |
| `update` | `0` instalado, já atualizado ou cancelado; `1` erro |

## Instalação

| Método | Destino padrão |
|---|---|
| `scripts/install.sh` | `~/.local/bin/codectx` |
| `scripts/install.ps1` | `%LOCALAPPDATA%\codectx\codectx.exe` |
| `make install` | `$(PREFIX)/bin`, com `PREFIX ?= ~/.local` |

Os scripts instalam sem sudo/admin, adicionam a pasta ao `PATH` e, se o
`codectx` já existir, atualizam no mesmo caminho. `scripts/upgrade-to-1.4.*`
são atalhos mantidos por compatibilidade.

## Release

Tag `v*` → workflow `.github/workflows/release.yml`: build Linux (x86_64,
aarch64, arm32 em containers Debian) e Windows (mingw-w64, estático), gera
`SHA256SUMS` e publica tudo na GitHub Release.

## Validação

```bash
make check     # warnings como erro
make test      # suíte table-driven
make sanitize  # ASan/UBSan
make
```
