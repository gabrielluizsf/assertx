$ErrorActionPreference = "Stop"

$repo = "gabrielluizsf/assertx"
$api = "https://api.github.com/repos/$repo/releases/latest"

Write-Host "🔍 Detectando sistema..."

$arch = if ([Environment]::Is64BitOperatingSystem) { "amd64" } else { "386" }
$asset = "assertx-windows.zip"

Write-Host "💻 Windows"
Write-Host "🧠 ARCH: $arch"

# ===== PEGAR URL =====
Write-Host "📦 Buscando release..."

$json = Invoke-RestMethod -Uri $api
$url = $json.assets | Where-Object { $_.name -eq $asset } | Select-Object -ExpandProperty browser_download_url

if (-not $url) {
    Write-Host "❌ Binário não encontrado"
    exit 1
}

# ===== DOWNLOAD =====
$tmp = New-Item -ItemType Directory -Path ([System.IO.Path]::GetTempPath() + [System.Guid]::NewGuid())
Set-Location $tmp

Write-Host "⬇️ Baixando..."
Invoke-WebRequest $url -OutFile $asset

# ===== EXTRAIR =====
Write-Host "📦 Extraindo..."
Expand-Archive $asset -Force

# ===== INSTALAÇÃO =====
$installDir = "$env:LOCALAPPDATA\AssertX"

Write-Host "📁 Instalando em $installDir"
New-Item -ItemType Directory -Force -Path $installDir | Out-Null

Move-Item "assertx.exe" "$installDir\assertx.exe" -Force

# ===== PATH =====
$path = [Environment]::GetEnvironmentVariable("Path", "User")

if ($path -notlike "*$installDir*") {
    Write-Host "🔧 Adicionando ao PATH..."
    [Environment]::SetEnvironmentVariable("Path", "$path;$installDir", "User")
}

# ===== BAIXAR xassert.h =====
Write-Host "⬇️ Baixando xassert.h..."
$originalDir = (Get-Location).Path
Set-Location $PWD

Invoke-WebRequest "https://raw.githubusercontent.com/$repo/main/tests/xassert.h" -OutFile "$originalDir\xassert.h"

# ===== FINAL =====
Write-Host ""
Write-Host "🧪 Testando..."

if (Get-Command assertx -ErrorAction SilentlyContinue) {
    Write-Host "✅ Instalado com sucesso!"
} else {
    Write-Host "⚠️ Reinicie o terminal"
}

Write-Host ""
Write-Host "📄 xassert.h salvo em: $originalDir"
Write-Host "🎉 Pronto! Use: assertx ./tests"