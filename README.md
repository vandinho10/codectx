# CodeContext CLI (`codectx`)

**CodeContext CLI** é uma CLI avançada de processamento de arquivos que moderniza o clássico comando `cat`. Construído com uma arquitetura de máquina de estados em C++17, ele permite aos desenvolvedores varrer diretórios recursivamente utilizando filtros precisos de inclusão e exclusão.

O utilitário já vem pré-configurado com regras inteligentes para ignorar pastas de dependências (como `.git`, `node_modules` e `venv`) e arquivos não-legíveis (binários e imagens), entregando uma saída de texto limpa, formatada e pronta para code review ou para alimentar modelos de inteligência artificial (LLMs).

---

## 🛠️ Compilação

O único pré-requisito é um compilador C++ (como `g++` ou `clang++`) que suporte o padrão **C++17**.

```bash
make              # build de release (otimizado, v1.3.0)
```

Você também pode compilar manualmente:

```bash
g++ -std=c++17 -O2 -o codectx main.cpp codectx.cpp
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
RESUMO: 106 ok, 0 falhas
```

Para validar manualmente o comportamento do binário:

```bash
./codectx --version           # codectx v1.3.0
./codectx --help              # ajuda
make version                  # codectx v1.3.0 (via Makefile)
```

---

## 📦 Instalação (Uso Global)

```bash
sudo make install             # instala em /usr/local/bin/codectx
codectx --help
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
├── tests/
│   ├── test_framework.hpp   # mini framework de testes (registro + CHECK)
│   └── codectx_tests.cpp    # suíte table-driven (unitários + integração)
├── Makefile
├── CHANGELOG.md
└── Readme.md
```

---

## 🔖 Versionamento

- **SemVer:** `MAJOR.MINOR.PATCH` (definido no Makefile e injetado via `-DCODECTX_VERSION`).
- **Conventional Commits:** mensagens `feat:`, `fix:`, `docs:`, `chore:`, `refactor:`.
- **Changelog:** em `CHANGELOG.md` seguindo o padrão Keep a Changelog.
