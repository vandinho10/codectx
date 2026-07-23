# CodeContext CLI (`codectx`)

**CodeContext CLI** é uma CLI avançada de processamento de arquivos que moderniza o clássico comando `cat`. Construído com uma arquitetura de máquina de estados em C++, ele permite aos desenvolvedores varrer diretórios recursivamente utilizando filtros precisos de inclusão e exclusão.

O utilitário já vem pré-configurado com regras inteligentes para ignorar pastas de dependências (como `.git`, `node_modules` e `venv`) e arquivos não-legíveis (como binários e imagens), entregando uma saída de texto limpa, formatada e pronta para code review ou para alimentar modelos de inteligência artificial (LLMs).

---

## 🛠️ Compilação

Você pode compilar o projeto de duas formas. O único pré-requisito é ter um compilador C++ (como `g++` ou `clang++`) que suporte o padrão **C++17**.

### Opção 1: Usando Makefile (Recomendado)
Se você estiver em um ambiente Linux/MacOS/WSL com o `make` instalado, basta rodar o comando na raiz do projeto:
```bash
make
```

### Opção 2: One-liner (Linha única no Terminal)
Caso não queira usar o Makefile, você pode compilar o arquivo fonte diretamente com o GCC:
```bash
g++ -std=c++17 -O3 -o codectx main.cpp
```
*(A flag `-O3` garante que o binário gerado seja altamente otimizado para velocidade).*

---

## 📦 Instalação (Uso Global)

Para conseguir rodar o comando `codectx` em qualquer diretório do seu terminal sem precisar apontar para o caminho completo, você precisa mover o binário compilado para uma pasta que esteja no seu `$PATH` (como `/usr/local/bin`).

Execute o seguinte comando:
```bash
sudo mv codectx /usr/local/bin/
```
*(Após isso, você pode excluir a pasta do código-fonte ou mantê-la caso queira fazer modificações futuras).*

Para verificar se a instalação funcionou, digite:
```bash
codectx --help
```

---

## 🚀 Como Usar

A CLI `codectx` utiliza flags para refinar exatamente o que você deseja extrair:

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

## ⚙️ Principais Funcionalidades

* 🎯 **Filtros Avançados:** Selecione exatamente o que quer ver usando *Whitelists* de arquivos (`-f`) e extensões (`-e`).
* 🛡️ **Blacklist Inteligente:** Ignora automaticamente bibliotecas pesadas, arquivos minificados e mídias visuais.
* 🧩 **Multi-Targeting:** Expanda seu escopo de busca dinamicamente (`-a`).
* 📄 **Pronto para Exportação:** Relatórios com delimitadores claros e organizados para cada script processado.
