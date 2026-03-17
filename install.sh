#!/usr/bin/env sh

set -e

REPO="gabrielluizsf/assertx"
API_URL="https://api.github.com/repos/$REPO/releases/latest"

# ===== DETECTAR LANG =====
LANG_SYS="${LC_ALL:-${LANG:-en}}"

case "$LANG_SYS" in
    pt_BR*|pt_PT*) LANG_MODE="pt" ;;
    *) LANG_MODE="en" ;;
esac

# ===== MENSAGENS =====
msg() {
    key="$1"
    case "$LANG_MODE:$key" in
        pt:detect) echo "🔍 Detectando sistema..." ;;
        en:detect) echo "🔍 Detecting system..." ;;

        pt:dep_missing) echo "❌ Dependência ausente:" ;;
        en:dep_missing) echo "❌ Missing dependency:" ;;

        pt:unsupported_os) echo "❌ Sistema não suportado:" ;;
        en:unsupported_os) echo "❌ Unsupported system:" ;;

        pt:unsupported_arch) echo "❌ Arquitetura não suportada:" ;;
        en:unsupported_arch) echo "❌ Unsupported architecture:" ;;

        pt:fetch) echo "📦 Buscando release..." ;;
        en:fetch) echo "📦 Fetching release..." ;;

        pt:download) echo "⬇️ Baixando..." ;;
        en:download) echo "⬇️ Downloading..." ;;

        pt:extract) echo "📦 Extraindo..." ;;
        en:extract) echo "📦 Extracting..." ;;

        pt:install) echo "📁 Instalando em:" ;;
        en:install) echo "📁 Installing to:" ;;

        pt:path_add) echo "🔧 Adicionando ao PATH em" ;;
        en:path_add) echo "🔧 Adding to PATH in" ;;

        pt:test) echo "🧪 Testando..." ;;
        en:test) echo "🧪 Testing..." ;;

        pt:success) echo "✅ Instalado com sucesso!" ;;
        en:success) echo "✅ Installed successfully!" ;;

        pt:restart) echo "⚠️ Reinicie o terminal ou rode:" ;;
        en:restart) echo "⚠️ Restart terminal or run:" ;;

        pt:not_found) echo "❌ Não foi possível encontrar o binário:" ;;
        en:not_found) echo "❌ Could not find binary:" ;;

        pt:download_header) echo "⬇️ Baixando xassert.h..." ;;
        en:download_header) echo "⬇️ Downloading xassert.h..." ;;

        pt:done) echo "🎉 Pronto! Use:" ;;
        en:done) echo "🎉 Done! Use:" ;;
    esac
}

msg detect

# ===== DEPENDÊNCIAS =====
require() {
    command -v "$1" >/dev/null 2>&1 || {
        msg dep_missing
        echo "$1"
        exit 1
    }
}

require curl
require grep
require cut

# ===== DETECÇÃO =====
OS="$(uname -s)"
ARCH="$(uname -m)"

case "$OS" in
    Linux*)   PLATFORM="linux" ;;
    Darwin*)  PLATFORM="macos" ;;
    MINGW*|MSYS*|CYGWIN*) PLATFORM="windows" ;;
    *)
        msg unsupported_os
        echo "$OS"
        exit 1
        ;;
esac

case "$ARCH" in
    x86_64|amd64) ARCH="amd64" ;;
    aarch64|arm64) ARCH="arm64" ;;
    *)
        msg unsupported_arch
        echo "$ARCH"
        exit 1
        ;;
esac

echo "💻 OS: $PLATFORM"
echo "🧠 ARCH: $ARCH"

# ===== ARQUIVO =====
if [ "$PLATFORM" = "windows" ]; then
    ARCHIVE_NAME="assertx-windows.zip"
else
    ARCHIVE_NAME="assertx-$PLATFORM.tar.gz"
fi

msg fetch

DOWNLOAD_URL=$(curl -fsSL "$API_URL" | grep "browser_download_url" | grep "$ARCHIVE_NAME" | cut -d '"' -f 4)

if [ -z "$DOWNLOAD_URL" ]; then
    msg not_found
    echo "$ARCHIVE_NAME"
    exit 1
fi

# ===== DOWNLOAD =====
msg download
mkdir -p tests
ORIGINAL_DIR="$(pwd)/tests"
TMP_DIR="$(mktemp -d)"
cd "$TMP_DIR"

curl -fsSL "$DOWNLOAD_URL" -o "$ARCHIVE_NAME"

# ===== EXTRAÇÃO =====
msg extract

if [ "$PLATFORM" = "windows" ]; then
    require unzip
    unzip -o "$ARCHIVE_NAME"
    BIN_NAME="assertx.exe"
else
    require tar
    tar -xzf "$ARCHIVE_NAME"
    BIN_NAME="assertx"
fi

# ===== HEADER =====
msg download_header
curl -fsSL "https://raw.githubusercontent.com/$REPO/main/tests/xassert.h" -o "$ORIGINAL_DIR/xassert.h"

# ===== INSTALAÇÃO =====
if [ "$PLATFORM" = "windows" ]; then
    INSTALL_DIR="$HOME/.assertx/bin"
else
    INSTALL_DIR="/usr/local/bin"
    USE_SUDO=""

    if [ ! -w "$INSTALL_DIR" ]; then
        INSTALL_DIR="$HOME/.local/bin"
        mkdir -p "$INSTALL_DIR"
    else
        USE_SUDO="sudo"
    fi
fi

msg install
echo "$INSTALL_DIR"

mkdir -p "$INSTALL_DIR"

if [ "$PLATFORM" = "windows" ]; then
    mv "$BIN_NAME" "$INSTALL_DIR/$BIN_NAME"
else
    $USE_SUDO mv "$BIN_NAME" "$INSTALL_DIR/$BIN_NAME"
    $USE_SUDO chmod +x "$INSTALL_DIR/$BIN_NAME"
fi

# ===== PATH =====
add_to_path() {
    SHELL_RC="$HOME/.profile"

    [ -f "$HOME/.bashrc" ] && SHELL_RC="$HOME/.bashrc"
    [ -f "$HOME/.zshrc" ] && SHELL_RC="$HOME/.zshrc"

    if ! grep -q "$INSTALL_DIR" "$SHELL_RC"; then
        msg path_add
        echo "$SHELL_RC"
        echo "\nexport PATH=\"$INSTALL_DIR:\$PATH\"" >> "$SHELL_RC"
    fi
}

if [ "$PLATFORM" != "windows" ]; then
    case ":$PATH:" in
        *":$INSTALL_DIR:"*) ;;
        *) add_to_path ;;
    esac
fi

# ===== FINAL =====
echo ""
msg test

if command -v assertx >/dev/null 2>&1; then
    msg success
    echo "📍 $(command -v assertx)"
else
    msg restart
    echo "export PATH=\"$INSTALL_DIR:\$PATH\""
fi

echo ""
echo "📄 xassert.h → $ORIGINAL_DIR"
msg done
echo "assertx ./tests"