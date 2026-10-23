#!/usr/bin/env bash
# =============================================================================
# codectx — instalador / atualizador externo (Linux)
#
# Se o codectx já estiver instalado, atualiza no mesmo caminho (backup .old).
# Se não estiver, instala em ~/.local/bin (local do usuário, sem sudo), o que
# permite ao `codectx update` se substituir depois sem pedir permissão.
#
# Uso:
#   curl -fsSL https://raw.githubusercontent.com/vandinho10/codectx/main/scripts/install.sh | bash
#   # ou, a partir do clone:
#   ./scripts/install.sh
#   ./scripts/install.sh /usr/local/bin/codectx      # caminho explícito
#
# Variáveis:
#   CODECTX_BIN              caminho explícito do binário (instala/atualiza nele)
#   CODECTX_INSTALL_DIR      pasta da instalação nova (padrão: ~/.local/bin)
#   CODECTX_REPO             owner/repo (padrão: vandinho10/codectx)
#   CODECTX_TAG              tag fixa (ex.: v1.4.1); padrão = última release
#   CODECTX_YES=1            não pergunta confirmação ao atualizar
#   CODECTX_NO_MODIFY_PATH=1 não altera ~/.bashrc / ~/.zshrc / ~/.profile
# =============================================================================
set -euo pipefail

REPO="${CODECTX_REPO:-vandinho10/codectx}"
API="https://api.github.com/repos/${REPO}"
YES="${CODECTX_YES:-0}"
TAG="${CODECTX_TAG:-}"
INSTALL_DIR="${CODECTX_INSTALL_DIR:-$HOME/.local/bin}"

ok()   { printf '  \033[32mOK\033[0m    %s\n' "$*"; }
info() { printf '  \033[36m→\033[0m     %s\n' "$*"; }
warn() { printf '  \033[33m!\033[0m     %s\n' "$*"; }
die()  { printf '  \033[31mERRO\033[0m  %s\n' "$*" >&2; exit 1; }

need() {
  command -v "$1" >/dev/null 2>&1 || die "comando obrigatorio nao encontrado: $1"
}

# ---------- localizar binário ----------
# Imprime o caminho do binário instalado, ou nada se não houver instalação.
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
    "$INSTALL_DIR/codectx" \
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
      die "macOS ainda nao tem binario nas releases. Compile com: make install"
      ;;
    mingw*|msys*|cygwin*)
      die "Use o script PowerShell: scripts/install.ps1"
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

# ---------- PATH ----------
# Garante que a pasta do binário esteja no PATH das próximas sessões.
garantir_path() {
  local dir="$1"
  case ":$PATH:" in
    *":$dir:"*) return ;;
  esac

  if [[ "${CODECTX_NO_MODIFY_PATH:-0}" == "1" ]]; then
    warn "$dir nao esta no PATH. Adicione: export PATH=\"$dir:\$PATH\""
    return
  fi

  local rc linha rcs=()
  linha="export PATH=\"$dir:\$PATH\"  # codectx"
  for rc in "$HOME/.bashrc" "$HOME/.zshrc"; do
    [[ -f "$rc" ]] && rcs+=("$rc")
  done
  # Sem .bashrc/.zshrc: usa ~/.profile (lido por shells de login)
  (( ${#rcs[@]} )) || rcs=("$HOME/.profile")
  for rc in "${rcs[@]}"; do
    grep -qsF "$linha" "$rc" && continue
    printf '\n%s\n' "$linha" >>"$rc"
    info "PATH adicionado em $rc"
  done
  warn "Abra um novo terminal (ou rode: export PATH=\"$dir:\$PATH\") para usar 'codectx'"
}

# ---------- main ----------
main() {
  need curl
  need uname

  local BIN ARTEFATO DEST_DIR TMP SUMS_URL ASSET_URL TAG_NAME REMOTE_VER LOCAL_VER NOVO=0
  BIN="$(encontrar_binario "${1:-}")"
  if [[ -z "$BIN" || ! -e "$BIN" ]]; then
    # Instalação nova: caminho explícito inexistente ou pasta padrão
    [[ -n "$BIN" ]] || BIN="$INSTALL_DIR/codectx"
    NOVO=1
  fi
  DEST_DIR="$(dirname "$BIN")"

  ARTEFATO="$(nome_artefato)"
  if (( NOVO )); then
    LOCAL_VER="nao instalado"
  else
    LOCAL_VER="$(versao_local "$BIN")"
  fi

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

  if (( ! NOVO )); then
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
  fi

  TMP="$(mktemp "${TMPDIR:-/tmp}/codectx-install.XXXXXX")"
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

  # Pasta da instalação nova (ex.: ~/.local/bin pode não existir ainda)
  if [[ ! -d "$DEST_DIR" ]]; then
    mkdir -p "$DEST_DIR" 2>/dev/null || die "nao foi possivel criar $DEST_DIR"
  fi

  # Backup + replace (sudo se necessario)
  install_cmd() {
    if [[ -w "$DEST_DIR" ]]; then
      "$@"
    else
      need sudo
      warn "sem escrita em $DEST_DIR — usando sudo ('codectx update' tambem vai precisar)"
      sudo "$@"
    fi
  }

  if (( ! NOVO )); then
    install_cmd cp -f "$BIN" "${BIN}.old" || warn "nao foi possivel criar backup .old"
  fi
  install_cmd mv -f "$TMP" "$BIN"
  install_cmd chmod 755 "$BIN"
  trap - EXIT

  if (( NOVO )); then
    ok "Instalado: $BIN"
    garantir_path "$DEST_DIR"
  else
    ok "Atualizado: $BIN"
    info "Backup:    ${BIN}.old (se criado)"
  fi
  info "Versao instalada: $("$BIN" --version 2>/dev/null || echo '?')"
  info "Proximas atualizacoes: codectx update"
}

main "$@"
