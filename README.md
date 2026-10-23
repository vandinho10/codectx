# CodeContext CLI (`codectx`)

**CodeContext CLI** é uma CLI avançada de processamento de arquivos que moderniza o clássico comando `cat`. Construído com uma arquitetura de máquina de estados em C++17, ele permite aos desenvolvedores varrer diretórios recursivamente utilizando filtros precisos de inclusão e exclusão.

O utilitário já vem pré-configurado com regras inteligentes para ignorar pastas de dependências (como `.git`, `node_modules` e `venv`) e arquivos não-legíveis (binários e imagens), entregando uma saída de texto limpa, formatada e pronta para code review ou para alimentar modelos de inteligência artificial (LLMs).

---

## 🛠️ Compilação

O único pré-requisito é um compilador C++ (como `g++` ou `clang++`) que suporte o padrão **C++17**.

```bash
make              # build de release (otimizado, v1.4.0)
```

Você também pode compilar manualmente:

```bash
g++ -std=c++17 -O2 -o codectx main.cpp codectx.cpp config.cpp update.cpp
```

Cross-compile para Windows (binário 100% estático, via mingw-w64):

```bash
make TARGET_OS=Windows_NT CXX=x86_64-w64-mingw32-g++-posix all
```

---

## ✅ Verificação, Testes e Sanitizers

A arquitetura de qualidade aplicada segue o ciclo: **verificação estática -> testes unitários/integração -> sanitizers -> build release**.

```bash
make check        # warnings tratados como erro (-Werror) em todos os arquivos
make test         # compila e roda a suíte de testes table-driven (unitários + integração)
make sanitize     # compila os testes com AddressSanitizer + UndefinedBehaviorSanitizer e roda
```

Exemplo de saída da suíte:

```
RESUMO: 219 ok, 0 falhas
```

Para validar manualmente o comportamento do binário:

```bash
./codectx --version           # codectx v1.4.0
./codectx --help              # ajuda
make version                  # codectx v1.4.0 (via Makefile)
```

---

## 📥 Download (binários pré-compilados)

Cada tag `v*` dispara o workflow [`release.yml`](.github/workflows/release.yml),
que compila e publica os binários em
[GitHub Releases](https://github.com/vandinho10/codectx/releases/latest),
junto com o arquivo `SHA256SUMS`:

| Plataforma | Artefato |
|---|---|
| Linux x86_64 | `codectx-linux-x86_64` |
| Linux ARM64 | `codectx-linux-aarch64` |
| Linux ARMv7 (32 bits) | `codectx-linux-arm32` |
| Windows 64 bits | `codectx-windows-amd64.exe` |
| Windows 32 bits | `codectx-windows-x86.exe` |

Exemplo (Linux x86_64):

```bash
curl -fLO https://github.com/vandinho10/codectx/releases/latest/download/codectx-linux-x86_64
curl -fLO https://github.com/vandinho10/codectx/releases/latest/download/SHA256SUMS
sha256sum --check --ignore-missing SHA256SUMS
install -m 0755 codectx-linux-x86_64 ~/.local/bin/codectx
```

---

## 📦 Instalação a partir do código-fonte (Uso Global)

```bash
sudo make install             # instala em /usr/local/bin/codectx
codectx --help
```

---

## 🔄 Atualização

Requer [`curl`](https://curl.se/) no `PATH`. Consulta
[GitHub Releases](https://github.com/vandinho10/codectx/releases/latest) e
substitui o binário em execução (verifica `SHA256SUMS` quando disponível).

```bash
codectx update check     # só informa se há versão mais nova
codectx update           # baixa e instala (pede confirmação)
codectx update --yes     # instala sem prompt (scripts/CI)
```

Override do repositório: `CODECTX_UPDATE_REPO=owner/repo`.

### Atualizando a partir da v1.3.x

Versões anteriores à 1.4 não possuem `codectx update`. Use os scripts de
upgrade, que localizam o binário instalado, baixam o artefato correto e o
substituem no mesmo caminho (com backup `.old`):

```bash
# Linux
curl -fsSL https://raw.githubusercontent.com/vandinho10/codectx/main/scripts/upgrade-to-1.4.sh | bash
```

```powershell
# Windows (PowerShell)
irm https://raw.githubusercontent.com/vandinho10/codectx/main/scripts/upgrade-to-1.4.ps1 | iex
```

---

## 🚀 Como Usar

```bash
# Uso básico: varre a pasta atual inteira (ignorando node_modules, .git, binários, etc.)
codectx

# Filtrar apenas por extensões específicas (ex: apenas arquivos C++ e Headers)
codectx -e cpp hpp

# Buscar por arquivos exatos, mesmo que estejam em outros caminhos absolutos
codectx -f Dockerfile Makefile /home/user/meu_script.sh

# Adicionar outros diretórios à busca atual e salvar a saída em um arquivo txt
codectx -a /var/log/meu_app -o contexto_ia.txt
```

> Observação: opções multi-argumento (`-e`, `-f`, `-a`, `-n`) leem os argumentos em sequência até a próxima flag. Use `-t` (ou um caminho existente) para voltar a ler como alvo principal. Ex.: `codectx -e cpp -t src/`.

---

## ⚙️ Configuração Persistente

Salve suas preferências uma vez e rode `codectx` sem flags — no estilo do `git config`.

```bash
# Global (padrão): vale para qualquer diretório
codectx config set ext cpp hpp
codectx config add alvos /var/log/meu_app
codectx config set output contexto_ia.txt
codectx config list                 # lista a visão efetiva com origem [global]/[local]
codectx config get ext              # imprime os valores da chave
codectx config unset ext            # remove a chave
codectx config path                 # mostra os caminhos global e local

# Local: grava ./.codectx.conf no projeto atual (commitável)
codectx config --local set ext php js
```

**Precedência:** linha de comando > `.codectx.conf` local > config global > defaults.
Categorias não informadas na CLI são preenchidas pela config; `-a/--add` e
`-n/--not` sempre somam com o que já existe. Chaves aceitas: `alvos`, `ext`,
`file`, `add`, `not`, `output`, `no-rec`.

Formato do arquivo (`chave=valor`, uma linha por valor):

```ini
# ~/.config/codectx/config
alvos=src
ext=cpp
ext=hpp
output=contexto_ia.txt
```

---

## ⚙️ Principais Funcionalidades

* 🎯 **Filtros Avançados:** Selecione exatamente o que quer ver usando *Whitelists* de arquivos (`-f`) e extensões (`-e`).
* 🛡️ **Blacklist Inteligente:** Ignora automaticamente bibliotecas pesadas, arquivos minificados, mídias visuais e binários (`.bin`, `.o`, `.a`, `.so`, `.exe`, imagens, vídeos etc.).
* 🧩 **Multi-Targeting:** Expanda seu escopo de busca dinamicamente (`-a`).
* 📄 **Pronto para Exportação:** Relatórios com delimitadores claros e organizados para cada arquivo processado.

---

## 🗂️ Estrutura do Projeto

```
codectx/
├── main.cpp            # entry point (subcomando config + parse + execução)
├── codectx.hpp         # contrato público (Config, parse_args, ConcatenadorDeArquivos, NivelConfig)
├── codectx.cpp         # implementação (filtros, varredura, exclusões)
├── config.cpp          # configuração persistente (níveis global/local, subcomando config)
├── update.cpp          # subcomando update (GitHub Releases, SHA256SUMS, SemVer)
├── scripts/            # upgrade-to-1.4.sh / .ps1 (atualizador externo)
├── docs/               # documentação técnica
├── tests/
│   ├── test_framework.hpp   # mini framework de testes (registro + CHECK)
│   └── codectx_tests.cpp    # suíte table-driven (unitários + integração)
├── .github/workflows/release.yml   # build multiplataforma + publicação da release
├── Makefile
├── CHANGELOG.md
├── LICENSE / NOTICE
└── README.md
```

---

## 🔖 Versionamento

- **SemVer:** `MAJOR.MINOR.PATCH` (definido no Makefile e injetado via `-DCODECTX_VERSION`).
- **Conventional Commits:** mensagens `feat:`, `fix:`, `docs:`, `chore:`, `refactor:`.
- **Changelog:** em `CHANGELOG.md` seguindo o padrão Keep a Changelog.

---

## 📄 Licença

Distribuído sob a licença [MIT](LICENSE). Veja também [`NOTICE`](NOTICE).
