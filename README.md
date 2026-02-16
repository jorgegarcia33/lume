# Lume

A lightweight, terminal-based peer-to-peer (P2P) chat application written in C.

<details>
<summary><strong>Features</strong></summary>

- **P2P Communication**: Direct messaging between peers using TCP/IP.
- **Automatic Discovery**: Local network peer discovery via UDP beacons.
- **Terminal UI**: Interactive interface built with `ncurses`.
- **File Transfer**: Support for sending and receiving files over the network.
- **End-to-End Encryption**: All messages and file transfers are encrypted using X25519 key exchange and XSalsa20-Poly1305 authenticated encryption via `libsodium`. Encryption is fully transparent — no configuration or key management required.

</details>

<details>
<summary><strong>Requirements</strong></summary>

- GCC
- Make
- ncurses library
- pthread library
- libsodium library

</details>

<details>
<summary><strong>Installation</strong></summary>

#### Quick Install (Debian/Ubuntu)
The easiest way to install Lume is by running our automated script:

```bash
curl -sSL https://raw.githubusercontent.com/jorgegarcia33/lume/master/install.sh | bash
```

#### Manual Installation
1. Download the latest `lume.deb` package from the [Releases](https://github.com/jorgegarcia33/lume/releases) section.
2. Install the package using:
   ```bash
   sudo apt install ./lume.deb
   ```
3. Launch the application:
   ```bash
   lume <username> <port>
   ```

#### Build from Source
If you prefer to build the project manually:

```bash
make
sudo make install
```

</details>

<details>
<summary><strong>Usage</strong></summary>

```bash
lume <username> <port>
```

- **Start with arguments**: `lume <username> <port>`
- **Start with saved config**: `lume` (loads from `~/.config/lume/lume.conf`)
- **Configure interactively**: `lume config`

</details>

<details>
<summary><strong>Controls & Commands</strong></summary>

- <kbd>Arrow Keys (Up/Down)</kbd>: Cycle through the list of discovered peers in the network.
- <kbd>Type & Enter</kbd>: Send a text message to the currently selected peer.
- <kbd>/file &lt;path&gt;</kbd>: Send a file to the selected peer (e.g., `/file ./document.txt`).
- <kbd>ESC</kbd>: Exit the application.

</details>

<details>
<summary><strong>Configuration File</strong></summary>

You can easily create or update your configuration using the `config` command:

```bash
lume config
```

Alternatively, you can manually create `~/.config/lume/lume.conf` with the following format:

```ini
username=yourname
port=12345
```

Both fields are required. The application will only use the config file if you run `lume` without arguments.

</details>

<details>
<summary><strong>Contributing</strong></summary>

Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on how to contribute, report issues, or request features.

</details>
