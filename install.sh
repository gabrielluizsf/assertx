#!/usr/bin/env sh

set -e

REPO="gabrielluizsf/assertx"
API_URL="https://api.github.com/repos/$REPO/releases/latest"

echo "🔍 Detectando sistema..."

# ===== DEPENDÊNCIAS =====
require() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "❌ Dependência ausente: $1"
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
        echo "❌ Sistema não suportado: $OS"
        exit 1
        ;;
esac

case "$ARCH" in
    x86_64|amd64) ARCH="amd64" ;;
    aarch64|arm64) ARCH="arm64" ;;
    *)
        echo "❌ Arquitetura não suportada: $ARCH"
        exit 1
        ;;
esac

echo "💻 OS: $PLATFORM"
echo "🧠 ARCH: $ARCH"

# ===== DEFINIR ARQUIVO =====
if [ "$PLATFORM" = "windows" ]; then
    ARCHIVE_NAME="assertx-windows.zip"
else
    ARCHIVE_NAME="assertx-$PLATFORM.tar.gz"
fi

echo "📦 Buscando release..."

# ===== PEGAR URL (fallback sem jq) =====
DOWNLOAD_URL=$(curl -fsSL "$API_URL" | grep "browser_download_url" | grep "$ARCHIVE_NAME" | cut -d '"' -f 4)

if [ -z "$DOWNLOAD_URL" ]; then
    echo "❌ Não foi possível encontrar o binário: $ARCHIVE_NAME"
    exit 1
fi

# ===== DOWNLOAD =====
echo "⬇️ Baixando..."
ORIGINAL_DIR="$(pwd)"
TMP_DIR="$(mktemp -d)"
cd "$TMP_DIR"

curl -fsSL "$DOWNLOAD_URL" -o "$ARCHIVE_NAME"

# ===== EXTRAÇÃO =====
echo "📦 Extraindo..."

if [ "$PLATFORM" = "windows" ]; then
    require unzip
    unzip -o "$ARCHIVE_NAME"
    BIN_NAME="assertx.exe"
else
    require tar
    tar -xzf "$ARCHIVE_NAME"
    BIN_NAME="assertx"
fi

# ===== BAIXAR xassert.h =====
echo "⬇️ Baixando xassert.h..."
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

echo "📁 Instalando em: $INSTALL_DIR"
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
        echo "🔧 Adicionando ao PATH em $SHELL_RC"
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
echo "🧪 Testando..."

if command -v assertx >/dev/null 2>&1; then
    echo "✅ Instalado com sucesso!"
    echo "📍 $(command -v assertx)"
else
    echo "⚠️ Reinicie o terminal ou rode:"
    echo "export PATH=\"$INSTALL_DIR:\$PATH\""
fi

echo ""
echo "📄 xassert.c salvo em: $TMP_DIR"
echo "🎉 Pronto! Use: assertx ./tests"