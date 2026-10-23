# =============================================================================
# codectx — instalador / atualizador externo (Windows / PowerShell)
#
# Se o codectx já estiver instalado, atualiza no mesmo caminho (backup .old).
# Se não estiver, instala em %LOCALAPPDATA%\codectx (pasta do usuário, sem
# admin) e adiciona essa pasta ao PATH do usuário, o que permite ao
# `codectx update` se substituir depois sem pedir permissão.
#
# Uso (PowerShell):
#   irm https://raw.githubusercontent.com/vandinho10/codectx/main/scripts/install.ps1 | iex
#   .\scripts\install.ps1
#   .\scripts\install.ps1 -BinPath 'C:\Tools\codectx.exe'
#   .\scripts\install.ps1 -InstallDir 'D:\bin'
#   .\scripts\install.ps1 -Yes
#   .\scripts\install.ps1 -NoModifyPath
#   $env:CODECTX_TAG = 'v1.4.1'; .\scripts\install.ps1
# =============================================================================

[CmdletBinding()]
param(
    [string]$BinPath = $env:CODECTX_BIN,
    [string]$Repo = $(if ($env:CODECTX_REPO) { $env:CODECTX_REPO } else { 'vandinho10/codectx' }),
    [string]$Tag = $env:CODECTX_TAG,
    [string]$InstallDir = $(if ($env:CODECTX_INSTALL_DIR) { $env:CODECTX_INSTALL_DIR } else { Join-Path $env:LOCALAPPDATA 'codectx' }),
    [switch]$Yes,
    [switch]$NoModifyPath = $($env:CODECTX_NO_MODIFY_PATH -eq '1')
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

# Retorna o caminho do .exe instalado, ou $null se não houver instalação.
function Find-CodectxBinary {
    param([string]$Explicit)

    if ($Explicit) {
        if (Test-Path -LiteralPath $Explicit) {
            return (Resolve-Path -LiteralPath $Explicit).Path
        }
        return $Explicit
    }

    $cmd = Get-Command codectx -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Source) {
        return $cmd.Source
    }

    $candidates = @(
        (Join-Path $InstallDir 'codectx.exe'),
        (Join-Path $env:LOCALAPPDATA 'codectx\codectx.exe'),
        (Join-Path $env:USERPROFILE 'bin\codectx.exe'),
        (Join-Path $env:USERPROFILE '.local\bin\codectx.exe'),
        'C:\Tools\codectx.exe',
        'C:\Program Files\codectx\codectx.exe'
    )
    foreach ($c in $candidates) {
        if (Test-Path -LiteralPath $c) { return $c }
    }
    return $null
}

# Garante que a pasta esteja no PATH do usuário (registro) e da sessão atual.
function Add-ToUserPath([string]$Dir) {
    $norm = $Dir.TrimEnd('\')
    $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
    $parts = @()
    if ($userPath) { $parts = $userPath -split ';' | Where-Object { $_ } }
    $presente = $parts | Where-Object { $_.TrimEnd('\') -ieq $norm }

    if (-not $presente) {
        if ($NoModifyPath) {
            Write-Warn "$Dir nao esta no PATH. Adicione manualmente ou rode sem -NoModifyPath"
            return
        }
        [Environment]::SetEnvironmentVariable('Path', (($parts + $Dir) -join ';'), 'User')
        Write-Info "PATH do usuario atualizado: $Dir"
        Write-Warn 'Abra um novo terminal para usar codectx'
    }
    if (-not (($env:Path -split ';') | Where-Object { $_.TrimEnd('\') -ieq $norm })) {
        $env:Path = "$env:Path;$Dir"
    }
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

function Invoke-Install {
    $bin = Find-CodectxBinary -Explicit $BinPath
    $novo = $false
    if (-not $bin -or -not (Test-Path -LiteralPath $bin)) {
        # Instalação nova: caminho explícito inexistente ou pasta padrão
        if (-not $bin) { $bin = Join-Path $InstallDir 'codectx.exe' }
        $novo = $true
    }
    $artifact = Get-ArtifactName
    $localVer = if ($novo) { 'nao instalado' } else { Get-LocalVersion -Bin $bin }
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
            'User-Agent' = 'codectx-install-script'
        }
        $rel = Invoke-RestMethod -Uri $api -Headers $headers
        $Tag = $rel.tag_name
    }
    if (-not $Tag) { Die 'nao foi possivel obter a tag da release' }

    $remoteVer = $Tag.TrimStart('v')
    Write-Info "Remoto:   $Tag"

    if (-not $novo) {
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
    }

    $id = [guid]::NewGuid().ToString('N')
    $tmp = Join-Path $env:TEMP ("codectx-install-$id.exe")
    $tmpSums = Join-Path $env:TEMP ("codectx-install-$id.sums")
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

        $destDir = Split-Path -Parent $bin
        if (-not (Test-Path -LiteralPath $destDir)) {
            New-Item -ItemType Directory -Path $destDir -Force | Out-Null
        }

        $backup = "$bin.old"
        if (-not $novo) {
            try {
                Copy-Item -LiteralPath $bin -Destination $backup -Force
            } catch {
                Write-Warn "nao foi possivel criar backup .old (arquivo em uso?): $_"
            }
        }

        # Tenta substituir; se o .exe estiver em uso, deixa o novo ao lado.
        try {
            Move-Item -LiteralPath $tmp -Destination $bin -Force
            if ($novo) { Write-Ok "Instalado: $bin" } else { Write-Ok "Atualizado: $bin" }
        } catch {
            $side = "$bin.new.exe"
            Move-Item -LiteralPath $tmp -Destination $side -Force
            Write-Warn "Nao foi possivel sobrescrever o .exe em uso."
            Write-Info "Novo binario salvo em: $side"
            Write-Info "Feche terminais que usam codectx e rode:"
            Write-Host "  Move-Item -Force '$side' '$bin'"
            return 1
        }

        if ($novo) {
            Add-ToUserPath -Dir $destDir
        } elseif (Test-Path -LiteralPath $backup) {
            Write-Info "Backup:    $backup"
        }
        try {
            $nv = & $bin --version 2>$null
            Write-Info "Versao instalada: $nv"
        } catch {}
        Write-Info 'Proximas atualizacoes: codectx update'
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
    $code = Invoke-Install
} catch {
    Write-Host "  ERRO  $($_.Exception.Message)" -ForegroundColor Red
    $code = 1
}
# Só encerra o processo quando rodado como arquivo; sob `irm | iex` apenas retorna.
if ($PSCommandPath) { exit $code }
