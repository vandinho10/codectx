#!/usr/bin/env bash
# =============================================================================
# codectx — upgrade 1.3.x → 1.4.x (ou última v1.4.*)
#
# Localiza o binário instalado, baixa o artefato correto do GitHub Releases
# e substitui no mesmo caminho (com backup .old).
#
# Uso:
#   curl -fsSL https://raw.githubusercontent.com/vandinho10/codectx/main/scripts/upgrade-to-1.4.sh | bash
#   # ou, a partir do clone:
#   ./scripts/upgrade-to-1.4.sh
#   ./scripts/upgrade-to-1.4.sh /usr/local/bin/codectx
#   CODECTX_BIN=~/.local/bin/codectx ./scripts/upgrade-to-1.4.sh
#
# Variáveis:
#   CODECTX_BIN     caminho explícito do binário
#   CODECTX_REPO    owner/repo (padrão: vandinho10/codectx)
#   CODECTX_TAG     tag fixa (ex.: v1.4.0); padrão = última release
#   CODECTX_YES=1   não pergunta confirmação
# =============================================================================
set -euo pipefail

REPO="${CODECTX_REPO:-vandinho10/codectx}"
API="https://api.github.com/repos/${REPO}"
YES="${CODECTX_YES:-0}"
TAG="${CODECTX_TAG:-}"

ok()   { printf '  \033[32mOK\033[0m    %s\n' "$*"; }
info() { printf '  \033[36m→\033[0m     %s\n' "$*"; }
warn() { printf '  \033[33m!\033[0m     %s\n' "$*"; }
die()  { printf '  \033[31mERRO\033[0m  %s\n' "$*" >&2; exit 1; }

need() {
  command -v "$1" >/dev/null 2>&1 || die "comando obrigatorio nao encontrado: $1"
}

# ---------- localizar binário ----------
encontrar_binario() {
  if [[ -n "${1:-}" ]]; then
    printf '%s\n' "$1"
    return
  fi
  if [[ -n "${CODECTX_BIN:-}" ]]; then
    printf '%s\n' "$CODECTX_BIN"
    return
  fi

  local cand
  if cand="$(command -v codectx 2>/dev/null)" && [[ -n "$cand" ]]; then
    # resolve symlink
    if command -v readlink >/dev/null 2>&1; then
      cand="$(readlink -f "$cand" 2>/dev/null || printf '%s' "$cand")"
    fi
    printf '%s\n' "$cand"
    return
  fi

  for cand in \
    /usr/local/bin/codectx \
    /usr/bin/codectx \
    "$HOME/.local/bin/codectx" \
    "$HOME/bin/codectx"
  do
    if [[ -x "$cand" ]]; then
      printf '%s\n' "$cand"
      return
    fi
  done

  die "codectx nao encontrado no PATH nem em locais comuns.
  Informe o caminho: CODECTX_BIN=/caminho/para/codectx (ou como argumento do script)"
}

# ---------- plataforma ----------
nome_artefato() {
  local os arch
  os="$(uname -s | tr '[:upper:]' '[:lower:]')"
  arch="$(uname -m)"

  case "$os" in
    linux*)
      case "$arch" in
        x86_64|amd64)  printf 'codectx-linux-x86_64\n' ;;
        aarch64|arm64) printf 'codectx-linux-aarch64\n' ;;
        armv7*|armv6*|arm*) printf 'codectx-linux-arm32\n' ;;
        *) die "arch Linux nao suportada: $arch" ;;
      esac
      ;;
    darwin*)
      die "macOS ainda nao tem binario nas releases. Compile com: make && sudo make install"
      ;;
    mingw*|msys*|cygwin*)
      die "Use o script PowerShell: scripts/upgrade-to-1.4.ps1"
      ;;
    *)
      die "SO nao suportado: $os"
      ;;
  esac
}

# ---------- versão local ----------
versao_local() {
  local bin="$1" out
  if out="$("$bin" --version 2>/dev/null)"; then
    # "codectx v1.3.2" → 1.3.2
    out="${out##*v}"
    out="${out%%[[:space:]]*}"
    printf '%s\n' "$out"
  else
    printf 'desconhecida\n'
  fi
}

# major.minor.patch → comparável; retorna 0 se a < b
semver_lt() {
  local a="$1" b="$2"
  local a1 a2 a3 b1 b2 b3
  IFS=. read -r a1 a2 a3 <<<"${a%%-*}"
  IFS=. read -r b1 b2 b3 <<<"${b%%-*}"
  a1=${a1:-0}; a2=${a2:-0}; a3=${a3:-0}
  b1=${b1:-0}; b2=${b2:-0}; b3=${b3:-0}
  if (( a1 < b1 )); then return 0; fi
  if (( a1 > b1 )); then return 1; fi
  if (( a2 < b2 )); then return 0; fi
  if (( a2 > b2 )); then return 1; fi
  if (( a3 < b3 )); then return 0; fi
  return 1
}

# ---------- main ----------
main() {
  need curl
  need uname

  local BIN ARTEFATO DEST_DIR TMP SUMS_URL ASSET_URL TAG_NAME REMOTE_VER LOCAL_VER
  BIN="$(encontrar_binario "${1:-}")"
  [[ -e "$BIN" ]] || die "arquivo nao existe: $BIN"
  [[ -w "$(dirname "$BIN")" ]] || warn "sem escrita em $(dirname "$BIN") — pode precisar de sudo"

  ARTEFATO="$(nome_artefato)"
  LOCAL_VER="$(versao_local "$BIN")"

  info "Binario:  $BIN"
  info "Versao:   $LOCAL_VER"
  info "Artefato: $ARTEFATO"
  info "Repo:     $REPO"

  if [[ -z "$TAG" ]]; then
    info "Consultando ${API}/releases/latest ..."
    TAG_NAME="$(curl -fsSL \
      -H 'Accept: application/vnd.github+json' \
      -H 'X-GitHub-Api-Version: 2022-11-28' \
      "${API}/releases/latest" \
      | sed -n 's/.*"tag_name":[[:space:]]*"\([^"]*\)".*/\1/p' \
      | head -1)"
    [[ -n "$TAG_NAME" ]] || die "nao foi possivel obter tag_name da API"
  else
    TAG_NAME="$TAG"
  fi

  REMOTE_VER="${TAG_NAME#v}"
  info "Remoto:   $TAG_NAME"

  # Só avança se o remoto for 1.4+ (objetivo deste script) ou se for mais novo
  case "$REMOTE_VER" in
    1.4.*|1.[5-9].*|2.*) ;;
    *)
      warn "A release mais recente ($TAG_NAME) nao e 1.4+. Use CODECTX_TAG=v1.4.0"
      ;;
  esac

  if [[ "$LOCAL_VER" != "desconhecida" ]] && ! semver_lt "$LOCAL_VER" "$REMOTE_VER"; then
    ok "Ja esta em $LOCAL_VER (>= $REMOTE_VER). Nada a fazer."
    exit 0
  fi

  if [[ "$YES" != "1" ]]; then
    # Em `curl | bash` o stdin é o próprio script: lê a resposta do terminal.
    { : </dev/tty; } 2>/dev/null || die "sem terminal para confirmar. Rode com CODECTX_YES=1"
    printf 'Atualizar %s → %s em %s? [s/N] ' "$LOCAL_VER" "$TAG_NAME" "$BIN"
    read -r resp </dev/tty || true
    case "${resp:-}" in
      s|S|y|Y|sim|SIM|yes|YES) ;;
      *) echo "Cancelado."; exit 0 ;;
    esac
  fi

  DEST_DIR="$(dirname "$BIN")"
  TMP="$(mktemp "${TMPDIR:-/tmp}/codectx-upgrade.XXXXXX")"
  # shellcheck disable=SC2064
  trap 'rm -f "$TMP" "$TMP.sums" 2>/dev/null || true' EXIT

  ASSET_URL="https://github.com/${REPO}/releases/download/${TAG_NAME}/${ARTEFATO}"
  SUMS_URL="https://github.com/${REPO}/releases/download/${TAG_NAME}/SHA256SUMS"

  info "Baixando $ASSET_URL"
  curl -fsSL -o "$TMP" "$ASSET_URL" || die "download falhou"

  if curl -fsSL -o "$TMP.sums" "$SUMS_URL" 2>/dev/null; then
    info "Verificando SHA256..."
    local expected got
    expected="$(awk -v n="$ARTEFATO" '
      $2 == n || $2 == ("*" n) || $2 ~ ("/" n "$") { print tolower($1); exit }
    ' "$TMP.sums")"
    if [[ -n "$expected" ]]; then
      if command -v sha256sum >/dev/null 2>&1; then
        got="$(sha256sum "$TMP" | awk '{print tolower($1)}')"
      elif command -v shasum >/dev/null 2>&1; then
        got="$(shasum -a 256 "$TMP" | awk '{print tolower($1)}')"
      else
        warn "sha256sum/shasum ausente — pulando verificacao"
        got=""
      fi
      if [[ -n "$got" && "$got" != "$expected" ]]; then
        die "SHA256 nao confere (esperado $expected, obtido $got)"
      fi
      [[ -n "$got" ]] && ok "SHA256 OK"
    else
      warn "hash de $ARTEFATO nao esta em SHA256SUMS"
    fi
  else
    warn "SHA256SUMS ausente nesta release"
  fi

  chmod +x "$TMP"

  # Backup + replace (sudo se necessario)
  local install_cmd
  install_cmd() {
    if [[ -w "$DEST_DIR" ]]; then
      "$@"
    else
      need sudo
      sudo "$@"
    fi
  }

  if [[ -e "$BIN" ]]; then
    install_cmd cp -f "$BIN" "${BIN}.old" || warn "nao foi possivel criar backup .old"
  fi
  install_cmd mv -f "$TMP" "$BIN"
  install_cmd chmod 755 "$BIN"
  trap - EXIT

  ok "Atualizado: $BIN"
  info "Backup:    ${BIN}.old (se criado)"
  if command -v "$BIN" >/dev/null 2>&1 || [[ -x "$BIN" ]]; then
    info "Nova versao: $("$BIN" --version 2>/dev/null || echo '?')"
  fi
  info "Teste: codectx update check"
}

main "$@"
