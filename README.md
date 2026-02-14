<p align="center">
  <img src="images/banner.svg" alt="BitrateSceneSwitch Banner" width="100%">
</p>

<p align="center">
  <a href="https://github.com/sniffingpickles/BitrateSceneSwitch/releases"><img src="https://img.shields.io/github/v/release/sniffingpickles/BitrateSceneSwitch?style=flat-square" alt="Release"></a>
  <a href="https://github.com/sniffingpickles/BitrateSceneSwitch/actions"><img src="https://img.shields.io/github/actions/workflow/status/sniffingpickles/BitrateSceneSwitch/build.yml?style=flat-square" alt="Build"></a>
  <a href="https://github.com/sniffingpickles/BitrateSceneSwitch/blob/main/LICENSE"><img src="https://img.shields.io/github/license/sniffingpickles/BitrateSceneSwitch?style=flat-square" alt="License"></a>
</p>

<p align="center">
  <i>This plugin was heavily inspired by <a href="https://github.com/NOALBS/nginx-obs-automatic-low-bitrate-switching">NOALBS</a>. Huge thanks to the NOALBS team for their amazing work!</i>
</p>

---

<img src="images/header-features.svg" alt="Features" width="100%">

- **Automatic Scene Switching** - Switch scenes based on bitrate thresholds
- **Multiple Server Support** - Works with BELABOX, NGINX, SLS, MediaMTX, and more
- **RTT Monitoring** - SRT round-trip-time based switching
- **Twitch Chat Integration** - Control scenes via chat commands
- **Configurable Triggers** - Set low bitrate, offline, and RTT thresholds
- **Optional Scenes** - Starting, ending, privacy, and refresh scenes

---

<img src="images/header-screenshots.svg" alt="Screenshots" width="100%">

<p align="center">
  <img src="images/screenshot-settings.svg" alt="Settings Dialog" width="80%">
</p>

<p align="center">
  <img src="images/screenshot-servers.svg" alt="Server Configuration" width="80%">
</p>

---

<img src="images/header-installation.svg" alt="Installation" width="100%">

### Windows

1. Download the latest release from [**Releases**](https://github.com/sniffingpickles/BitrateSceneSwitch/releases)
2. Extract the zip file
3. Copy contents to your OBS installation:
   - `BitrateSceneSwitch.dll` → `C:\Program Files\obs-studio\obs-plugins\64bit\`
4. Restart OBS Studio

---

<img src="images/header-usage.svg" alt="Usage" width="100%">

1. Open OBS Studio
2. Go to **Tools** → **Bitrate Scene Switch**
3. Configure your stream server and thresholds
4. Select your scenes and click **Save**

---

<img src="images/header-chat-commands.svg" alt="Chat Commands" width="100%">

Control the switcher via Twitch chat (requires setup in Chat tab):

| Command | Description |
|---------|-------------|
| `!live` | Switch to Live scene |
| `!low` | Switch to Low scene |
| `!brb` | Switch to BRB/Offline scene |
| `!ss <name>` | Switch to any scene (case-insensitive) |
| `!refresh` | Refresh scene (fix issues) |
| `!status` | Show current bitrate status |
| `!trigger` | Force switch check |

---

<img src="images/header-servers.svg" alt="Supported Servers" width="100%">

| Server | Stats URL Example |
|--------|-------------------|
| **BELABOX** | `https://cloud.belabox.net/stats` |
| **NGINX RTMP** | `http://localhost:8080/stat` |
| **SRT Live Server** | `http://localhost:8181/stats` |
| **MediaMTX** | `http://localhost:9997/v3/paths/get/` |
| **Node Media Server** | `http://localhost:8000/api/streams` |
| **Nimble** | `http://localhost:8082` |
| **IRLHosting** | Your server stats URL |

---

<img src="images/header-building.svg" alt="Building" width="100%">

```bash
cmake -B build -S .
cmake --build build --config Release
```

---

<img src="images/header-license.svg" alt="License" width="100%">

GPL-2.0

---

<p align="center">
  <sub>v1.0.0 | Powered by <a href="https://irlhosting.com"><b>IRL</b><span style="color: #22d3ee;">Hosting</span></a></sub>
</p>
