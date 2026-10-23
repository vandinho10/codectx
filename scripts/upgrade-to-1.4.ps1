# =============================================================================
# codectx — upgrade 1.3.x → 1.4.x (Windows / PowerShell)
#
# Localiza o .exe instalado, baixa o artefato correto do GitHub Releases
# e substitui no mesmo caminho (backup .old).
#
# Uso (PowerShell):
#   irm https://raw.githubusercontent.com/vandinho10/codectx/main/scripts/upgrade-to-1.4.ps1 | iex
#   .\scripts\upgrade-to-1.4.ps1
#   .\scripts\upgrade-to-1.4.ps1 -BinPath 'C:\Tools\codectx.exe'
#   .\scripts\upgrade-to-1.4.ps1 -Yes
#   $env:CODECTX_TAG = 'v1.4.0'; .\scripts\upgrade-to-1.4.ps1
# =============================================================================

[CmdletBinding()]
param(
    [string]$BinPath = $env:CODECTX_BIN,
    [string]$Repo = $(if ($env:CODECTX_REPO) { $env:CODECTX_REPO } else { 'vandinho10/codectx' }),
    [string]$Tag = $env:CODECTX_TAG,
    [switch]$Yes
)

$ErrorActionPreference = 'Stop'
# Barra de progresso deixa o Invoke-WebRequest muito lento no PowerShell 5.1
$ProgressPreference = 'SilentlyContinue'
# PowerShell 5.1 / .NET antigo pode não negociar TLS 1.2 por padrão (GitHub exige)
try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
} catch {}

function Write-Ok($m)   { Write-Host "  OK    $m" -ForegroundColor Green }
function Write-Info($m) { Write-Host "  ->    $m" -ForegroundColor Cyan }
function Write-Warn($m) { Write-Host "  !     $m" -ForegroundColor Yellow }
# Não usa `exit`: sob `irm | iex` isso fecharia a janela do PowerShell.
function Die($m)        { throw $m }

function Find-CodectxBinary {
    param([string]$Explicit)

    if ($Explicit -and (Test-Path -LiteralPath $Explicit)) {
        return (Resolve-Path -LiteralPath $Explicit).Path
    }

    $cmd = Get-Command codectx -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Source) {
        return $cmd.Source
    }

    $candidates = @(
        (Join-Path $env:LOCALAPPDATA 'codectx\codectx.exe'),
        (Join-Path $env:USERPROFILE 'bin\codectx.exe'),
        (Join-Path $env:USERPROFILE '.local\bin\codectx.exe'),
        'C:\Tools\codectx.exe',
        'C:\Program Files\codectx\codectx.exe'
    )
    foreach ($c in $candidates) {
        if (Test-Path -LiteralPath $c) { return $c }
    }

    Die @"
codectx.exe nao encontrado no PATH nem em locais comuns.
Informe: -BinPath 'C:\caminho\codectx.exe'
ou: `$env:CODECTX_BIN = 'C:\caminho\codectx.exe'
"@
}

function Get-ArtifactName {
    if ([Environment]::Is64BitOperatingSystem) {
        return 'codectx-windows-amd64.exe'
    }
    return 'codectx-windows-x86.exe'
}

function Get-LocalVersion([string]$Bin) {
    try {
        $out = & $Bin --version 2>$null
        if ($out -match 'v?(\d+\.\d+\.\d+)') { return $Matches[1] }
    } catch {}
    return 'desconhecida'
}

function Test-SemVerLessThan([string]$A, [string]$B) {
    $pa = ($A -split '-')[0] -split '\.' | ForEach-Object { [int]$_ }
    $pb = ($B -split '-')[0] -split '\.' | ForEach-Object { [int]$_ }
    while ($pa.Count -lt 3) { $pa += 0 }
    while ($pb.Count -lt 3) { $pb += 0 }
    for ($i = 0; $i -lt 3; $i++) {
        if ($pa[$i] -lt $pb[$i]) { return $true }
        if ($pa[$i] -gt $pb[$i]) { return $false }
    }
    return $false
}

function Invoke-Upgrade {
    $bin = Find-CodectxBinary -Explicit $BinPath
    $artifact = Get-ArtifactName
    $localVer = Get-LocalVersion -Bin $bin
    $Tag = $script:Tag

    Write-Info "Binario:  $bin"
    Write-Info "Versao:   $localVer"
    Write-Info "Artefato: $artifact"
    Write-Info "Repo:     $Repo"

    if (-not $Tag) {
        Write-Info "Consultando releases/latest ..."
        $api = "https://api.github.com/repos/$Repo/releases/latest"
        $headers = @{
            Accept = 'application/vnd.github+json'
            'User-Agent' = 'codectx-upgrade-script'
        }
        $rel = Invoke-RestMethod -Uri $api -Headers $headers
        $Tag = $rel.tag_name
    }
    if (-not $Tag) { Die 'nao foi possivel obter a tag da release' }

    $remoteVer = $Tag.TrimStart('v')
    Write-Info "Remoto:   $Tag"

    if ($localVer -ne 'desconhecida' -and -not (Test-SemVerLessThan $localVer $remoteVer)) {
        Write-Ok "Ja esta em $localVer (>= $remoteVer). Nada a fazer."
        return 0
    }

    if (-not $Yes) {
        $resp = Read-Host "Atualizar $localVer -> $Tag em ${bin}? [s/N]"
        if ("$resp" -notmatch '^[sSyY]') {
            Write-Host 'Cancelado.'
            return 0
        }
    }

    $id = [guid]::NewGuid().ToString('N')
    $tmp = Join-Path $env:TEMP ("codectx-upgrade-$id.exe")
    $tmpSums = Join-Path $env:TEMP ("codectx-upgrade-$id.sums")
    $assetUrl = "https://github.com/$Repo/releases/download/$Tag/$artifact"
    $sumsUrl  = "https://github.com/$Repo/releases/download/$Tag/SHA256SUMS"

    try {
        Write-Info "Baixando $assetUrl"
        Invoke-WebRequest -Uri $assetUrl -OutFile $tmp -UseBasicParsing

        # SHA256SUMS vem como application/octet-stream: no PowerShell 5.1 o
        # .Content seria byte[]. Grava em arquivo e lê como texto.
        $temSums = $true
        try {
            Invoke-WebRequest -Uri $sumsUrl -OutFile $tmpSums -UseBasicParsing
        } catch {
            $temSums = $false
            Write-Warn 'SHA256SUMS ausente ou inacessivel nesta release'
        }

        if ($temSums) {
            Write-Info 'Verificando SHA256...'
            $expected = $null
            foreach ($line in (Get-Content -LiteralPath $tmpSums)) {
                $line = $line.Trim()
                if (-not $line -or $line.StartsWith('#')) { continue }
                $parts = $line -split '\s+', 2
                if ($parts.Count -lt 2) { continue }
                $name = $parts[1].Trim().TrimStart('*')
                if ($name -eq $artifact -or $name.EndsWith("/$artifact")) {
                    $expected = $parts[0].Trim().ToLowerInvariant()
                    break
                }
            }
            if ($expected) {
                $hash = (Get-FileHash -LiteralPath $tmp -Algorithm SHA256).Hash.ToLowerInvariant()
                if ($hash -ne $expected) {
                    Die "SHA256 nao confere (esperado $expected, obtido $hash)"
                }
                Write-Ok 'SHA256 OK'
            } else {
                Write-Warn "hash de $artifact nao esta em SHA256SUMS"
            }
        }

        $backup = "$bin.old"
        if (Test-Path -LiteralPath $bin) {
            try {
                Copy-Item -LiteralPath $bin -Destination $backup -Force
            } catch {
                Write-Warn "nao foi possivel criar backup .old (arquivo em uso?): $_"
            }
        }

        # Tenta substituir; se o .exe estiver em uso, deixa o novo ao lado.
        try {
            Move-Item -LiteralPath $tmp -Destination $bin -Force
            Write-Ok "Atualizado: $bin"
        } catch {
            $side = "$bin.new.exe"
            Move-Item -LiteralPath $tmp -Destination $side -Force
            Write-Warn "Nao foi possivel sobrescrever o .exe em uso."
            Write-Info "Novo binario salvo em: $side"
            Write-Info "Feche terminais que usam codectx e rode:"
            Write-Host "  Move-Item -Force '$side' '$bin'"
            return 1
        }

        if (Test-Path -LiteralPath $backup) {
            Write-Info "Backup:    $backup"
        }
        try {
            $nv = & $bin --version 2>$null
            Write-Info "Nova versao: $nv"
        } catch {}
        Write-Info 'Teste: codectx update check'
        return 0
    }
    finally {
        foreach ($f in @($tmp, $tmpSums)) {
            if (Test-Path -LiteralPath $f) {
                Remove-Item -LiteralPath $f -Force -ErrorAction SilentlyContinue
            }
        }
    }
}

# --- main ---
$code = 1
try {
    $code = Invoke-Upgrade
} catch {
    Write-Host "  ERRO  $($_.Exception.Message)" -ForegroundColor Red
    $code = 1
}
# Só encerra o processo quando rodado como arquivo; sob `irm | iex` apenas retorna.
if ($PSCommandPath) { exit $code }
