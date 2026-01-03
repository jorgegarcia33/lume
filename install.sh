#!/bin/bash

# Exit on error
set -e

REPO="jorgegarcia33/lume"
echo "--- Lume Installer ---"

# 1. Check for dependencies
if ! command -v curl &> /dev/null; then
    echo "Error: curl is not installed. Please install it first."
    exit 1
fi

# 2. Get the latest release download URL for the .deb file
echo "Fetching latest version from GitHub..."
URL=$(curl -s https://api.github.com/repos/$REPO/releases/latest | grep "browser_download_url.*deb" | cut -d '"' -f 4)

if [ -z "$URL" ]; then
    echo "Error: Could not find a .deb file in the latest release."
    exit 1
fi

# 3. Download the package to a temporary location
echo "Downloading lume.deb..."
curl -L "$URL" -o /tmp/lume.deb

# 4. Install using apt (handles dependencies)
echo "Installing Lume..."
sudo apt update && sudo apt install -y /tmp/lume.deb

# 5. Cleanup
rm /tmp/lume.deb

echo "----------------------------------------"
echo "Installation complete! You can now run 'lume'."