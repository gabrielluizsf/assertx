$ErrorActionPreference = "Stop"

# ===== LANG =====
$lang = $env:LANG
if (-not $lang) { $lang = [System.Globalization.CultureInfo]::CurrentUICulture.Name }

if ($lang -like "pt-BR*" -or $lang -like "pt-PT*") {
    $L = "pt"
} else {
    $L = "en"
}

function Msg($key) {
    switch ("$L`:$key") {
        "pt:detect" { "🔍 Detectando sistema..." }
        "en:detect" { "🔍 Detecting system..." }

        "pt:fetch" { "📦 Buscando release..." }
        "en:fetch" { "📦 Fetching release..." }

        "pt:download" { "⬇️ Baixando..." }
        "en:download" { "⬇️ Downloading..." }

        "pt:extract" { "📦 Extraindo..." }
        "en:extract" { "📦 Extracting..." }

        "pt:install" { "📁 Instalando em" }
        "en:install" { "📁 Installing to" }

        "pt:path" { "🔧 Adicionando ao PATH..." }
        "en:path" { "🔧 Adding to PATH..." }

        "pt:success" { "✅ Instalado com sucesso!" }
        "en:success" { "✅ Installed successfully!" }

        "pt:restart" { "⚠️ Reinicie o terminal" }
        "en:restart" { "⚠️ Restart terminal" }

        "pt:header" { "⬇️ Baixando xassert.h..." }
        "en:header" { "⬇️ Downloading xassert.h..." }
    }
}

Write-Host (Msg "detect")

$repo = "gabrielluizsf/assertx"
$api = "https://api.github.com/repos/$repo/releases/latest"

Write-Host "💻 Windows"
Write-Host "🧠 ARCH: amd64"

Write-Host (Msg "fetch")

$json = Invoke-RestMethod -Uri $api
$url = $json.assets | Where-Object { $_.name -eq "assertx-windows.zip" } | Select-Object -ExpandProperty browser_download_url

if (-not $url) {
    Write-Host "❌ Binary not found"
    exit 1
}

$tmp = New-Item -ItemType Directory -Path ([System.IO.Path]::GetTempPath() + [guid]::NewGuid())
Set-Location $tmp

Write-Host (Msg "download")
Invoke-WebRequest $url -OutFile "assertx-windows.zip"

Write-Host (Msg "extract")
Expand-Archive "assertx-windows.zip" -Force

$installDir = "$env:LOCALAPPDATA\AssertX"

Write-Host (Msg "install") $installDir
New-Item -ItemType Directory -Force -Path $installDir | Out-Null

Move-Item "assertx.exe" "$installDir\assertx.exe" -Force

# ===== PATH =====
$path = [Environment]::GetEnvironmentVariable("Path", "User")

if ($path -notlike "*$installDir*") {
    Write-Host (Msg "path")
    [Environment]::SetEnvironmentVariable("Path", "$path;$installDir", "User")
}

# ===== HEADER (igual ao install.sh) =====
Write-Host (Msg "header")

# cria ./tests sem erro se já existir
$testsDir = Join-Path (Get-Location) "tests"
New-Item -ItemType Directory -Force -Path $testsDir | Out-Null

# salva dentro de tests/xassert.h
$headerPath = Join-Path $testsDir "xassert.h"

Invoke-WebRequest "https://raw.githubusercontent.com/$repo/main/tests/xassert.h" -OutFile $headerPath

Write-Host ""
Write-Host "📄 xassert.h → $headerPath"

# ===== FINAL =====
if (Get-Command assertx -ErrorAction SilentlyContinue) {
    Write-Host (Msg "success")
} else {
    Write-Host (Msg "restart")
}

Write-Host ""
Write-Host "🎉 Done! Use:"
Write-Host "assertx ./tests"