# PLANNER — codectx

> Planejamento da **atividade em execução**. Atualizado a cada novo planejamento.

## Atividade em execução: Documentação da v1.4.1

### Objetivo
Alinhar README, `docs/README.md`, TODO e PLANNER ao que entrou na v1.4.1
(correções do `codectx update`, instalador em local do usuário,
`make install` com `PREFIX`, `make uninstall`).

### Status
- [x] README (funcionalidades, contagem de testes, estrutura).
- [x] `docs/README.md` reescrito (CLI completa, subcomandos, códigos de saída,
  caminhos de config e instalação, release).
- [x] TODO e PLANNER atualizados.

## Atividade anterior: Instalador em local padrão — ✅ concluída

### Objetivo
Instalar o binário numa pasta do usuário (Linux `~/.local/bin`, Windows
`%LOCALAPPDATA%\codectx`) para que o `codectx update` consiga se substituir
sem sudo/admin.

### Decisões
- Script por plataforma (`scripts/install.sh` / `install.ps1`) que instala ou
  atualiza; `upgrade-to-1.4.*` viram atalhos para eles.
- `.deb`/MSI descartados: o gerenciador de pacotes passaria a ser dono do
  binário e conflitaria com o autoupdate. Scoop/winget ficam como opção futura.
- `make install` passa a usar `PREFIX ?= ~/.local`.

### Status
- [x] Scripts de instalação (testados em Linux; `.ps1` testado com pwsh no Linux).
- [x] Makefile (`PREFIX`, `uninstall`), README e CHANGELOG.
- [x] Validar `install.ps1` num Windows real (PATH do usuário) — v1.4.1 validada em Linux e Windows.

## Atividade anterior: Documentação da v1.4 e publicação de binários — ✅ concluída

### Objetivo
Atualizar o README para a v1.4.0 (download dos binários da release,
atualização via `codectx update` e scripts de upgrade, licença) e confirmar
a publicação automática dos binários pelo workflow `release.yml`.

### Status
- [x] Binários publicados em GitHub Releases via Actions (confirmado na v1.4.0).
- [x] README atualizado.

## Atividade anterior: Licenciamento (eixo A2) — LICENSE/NOTICE — ✅ concluída

### Objetivo
Declarar `LICENSE` (MIT) + `NOTICE` na raiz do repositório, atendendo ao
eixo de licenciamento do `docs/LICENCAS_DEPENDENCIAS.md` (A2).

### Resultado
`LICENSE` (MIT) e `NOTICE` presentes na raiz e referenciados no README.

## Atividade anterior: Padronização de documentação

### Objetivo
Garantir o conjunto `{README, CHANGELOG, TODO, PLANNER, docs/}`.

### Decisões
- Estrutura padrão do monorepo (SKILL.md seção 8); mantidos `Readme.md` e
  `CHANGELOG.md` já existentes.
