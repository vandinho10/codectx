#!/usr/bin/env bash
# =============================================================================
# codectx — upgrade 1.3.x/1.4.0 → última versão (mantido por compatibilidade)
#
# Substituído por scripts/install.sh, que instala ou atualiza. Este script só
# repassa para ele, para não quebrar quem já usa a URL antiga:
#   curl -fsSL https://raw.githubusercontent.com/vandinho10/codectx/main/scripts/upgrade-to-1.4.sh | bash
# =============================================================================
set -euo pipefail

REPO="${CODECTX_REPO:-vandinho10/codectx}"
LOCAL="$(dirname "${BASH_SOURCE[0]:-}")/install.sh"

# A partir do clone usa o install.sh ao lado; em `curl | bash` baixa do main.
if [[ -n "${BASH_SOURCE[0]:-}" && -f "$LOCAL" ]]; then
  exec bash "$LOCAL" "$@"
fi
curl -fsSL "https://raw.githubusercontent.com/${REPO}/main/scripts/install.sh" | bash -s -- "$@"
