# Documentação Técnica — codectx

## Entradas (CLI)

`codectx [opções] <alvo...>`

| Opção | Descrição |
|---|---|
| `-e, --ext <ext>` | Inclui arquivos por extensão (ex.: `-e cpp`) |
| `-h, --help` | Ajuda |
| `-v, --version` | Versão SemVer do binário |

## Comportamento

- Varredura recursiva de diretórios com filtros de inclusão/exclusão.
- Ignora por padrão: `.git`, `node_modules`, `venv`, binários/imagens e
  arquivos não-legíveis.
- Saída de texto formatada, pronta para code review ou consumo por LLMs.

## Saídas

- Texto processado no stdout (melhora o `cat` tradicional para contexto de
  código).

## Validação

```bash
make check   # warnings como erro
make test    # suíte table-driven
make sanitize  # ASan/UBSan
make
```
