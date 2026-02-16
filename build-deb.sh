#!/bin/bash
set -e

VERSION="1.1.0"
ARCH="amd64"
PKG_NAME="lume"
PKG_DIR="pkg/${PKG_NAME}_${VERSION}_${ARCH}"

echo "--- Building Lume v${VERSION} .deb package ---"

# 1. Clean build
make clean
make

# 2. Create package structure
rm -rf pkg/
mkdir -p "${PKG_DIR}/DEBIAN"
mkdir -p "${PKG_DIR}/usr/local/bin"

# 3. Copy binary
cp bin/lume "${PKG_DIR}/usr/local/bin/"

# 4. Create control file
cat > "${PKG_DIR}/DEBIAN/control" << EOF
Package: ${PKG_NAME}
Version: ${VERSION}
Section: net
Priority: optional
Architecture: ${ARCH}
Depends: libncurses6, libsodium23
Maintainer: Jorge Garcia <jorgegarcia@proton.me>
Homepage: https://github.com/jorgegarcia33/lume
Description: Lightweight terminal-based P2P chat with E2E encryption
 Peer-to-peer chat application with automatic discovery,
 file transfer, and end-to-end encryption using libsodium.
EOF

# 5. Build .deb
dpkg-deb --build "${PKG_DIR}" "${PKG_NAME}.deb"

echo "--- Package built: ${PKG_NAME}.deb ---"
