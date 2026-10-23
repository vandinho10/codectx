# =============================================================================
# codectx — upgrade 1.3.x/1.4.0 → última versão (mantido por compatibilidade)
#
# Substituído por scripts/install.ps1, que instala ou atualiza. Este script só
# repassa para ele, para não quebrar quem já usa a URL antiga:
#   irm https://raw.githubusercontent.com/vandinho10/codectx/main/scripts/upgrade-to-1.4.ps1 | iex
# =============================================================================

$local = if ($PSScriptRoot) { Join-Path $PSScriptRoot 'install.ps1' } else { $null }

# A partir do clone usa o install.ps1 ao lado (repassando os parâmetros);
# sob `irm | iex` baixa do main.
if ($local -and (Test-Path -LiteralPath $local)) {
    & $local @args
    if ($PSCommandPath) { exit $LASTEXITCODE }
} else {
    $repo = if ($env:CODECTX_REPO) { $env:CODECTX_REPO } else { 'vandinho10/codectx' }
    Invoke-RestMethod "https://raw.githubusercontent.com/$repo/main/scripts/install.ps1" | Invoke-Expression
}
