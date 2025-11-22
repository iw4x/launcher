<div align="center">

# IW4x Launcher
### Official launcher for the IW4x mod

#### [Installation Guide](https://docs.iw4x.io/get-started/quickstart/) • [Discord Server](https://discord.com/invite/pV2qJscTXf) • [Website](https://iw4x.io)

</div>


## 📦 Installation

> [!IMPORTANT]
> Only _legitimate copies_ of the game, **obtained from Steam**, are supported.  
> Copies from the **Microsoft Store are _incompatible_!**

1. Download Call of Duty: Modern Warfare 2 from [Steam](https://store.steampowered.com/app/10180/Call_of_Duty_Modern_Warfare_2_2009/)
2. Download the latest IW4x launcher
    - [Windows](https://github.com/iw4x/launcher/releases/latest/download/iw4x-launcher.exe)
    - [Linux](https://github.com/iw4x/launcher/releases/latest/download/iw4x-launcher-x86_64-unknown-linux-gnu.tar.gz)
3. Move the launcher to the game folder and run it

[Detailed installation instructions are available here](https://iw4x.io/install)

---

## 🔧 Config file

> [!TIP]
> The config file is located at <game_dir>/launcher/config.json

| Config Key | Default Value | Description |
|------------|---------------|-------------|
| `update_only` | `false` | Update only, don't launch IW4x |
| `skip_self_update` | `false` | Don't update the launcher |
| `force_update` | `false` | Force file re-check |
| `args` | `"-stdout"` | Arguments passed to the game, default is -stdout |
| `cdn_url` | `""` | Specify custom CDN url |
| `offline` | `false` | Run in offline mode |
| `testing` | `false` | Install from testing branch (IW4x & Launcher) |
| `disable_art` | `false` | Disable ASCII art |
| `dxvk` | `false` | Install DXVK for better AMD performance |

---

## ⚙️ Command line arguments

| Argument | Description |
|----------|-------------|
| `--path`, `-p` | Game install path |
| `--config`, `-c` | Custom config path, default is ./launcher/config.json |
| `--update`, `-u` | Update only, don't launch IW4x |
| `--force`, `-f` | Force file re-check |
| `--args` | Arguments passed to the game, default is -stdout |
| `--skip-self-update` | Don't update the launcher |
| `--ignore-required-files` | Don't check for required game files |
| `--skip-connectivity-check` | Disable CDN rating and use default CDN |
| `--offline` | Run in offline mode |
| `--testing` | Install from testing branch (IW4x & Launcher) |
| `--rate` | Rate CDN servers and print results |
| `--cdn-url` | Specify custom CDN url |
| `--disable-art` | Disable ASCII art |
| `--dxvk` | Install DXVK for better AMD performance |

---

## 🔨 Building from Source

See [CONTRIBUTING.md](CONTRIBUTING.md) for details.
