# Bitrate Scene Switch

An OBS Studio plugin for automatic scene switching based on stream server bitrate. This is a native OBS plugin implementation inspired by [NOALBS](https://github.com/NOALBS/nginx-obs-automatic-low-bitrate-switching).

## Features

- **Automatic Scene Switching**: Automatically switch between Normal, Low Bitrate, and Offline scenes based on your stream's bitrate
- **Multiple Server Support**: Monitor bitrate from various stream servers:
  - BELABOX Cloud
  - NGINX RTMP
  - SRT Live Server (SLS)
  - MediaMTX
- **Configurable Triggers**: Set custom thresholds for low bitrate and offline detection
- **RTT Monitoring**: For SRT streams, monitor Round Trip Time (RTT) as an additional trigger
- **Instant Recovery**: Optionally switch back to normal scene immediately when bitrate recovers
- **Easy Configuration**: Qt-based settings dialog accessible from OBS Tools menu

## Installation

### Windows
1. Download the latest release from the [Releases](https://github.com/sniffingpickles/BitrateSceneSwitch/releases) page
2. Extract the zip file
3. Copy the contents to your OBS Studio installation directory:
   - `obs-plugins/64bit/BitrateSceneSwitch.dll` → `C:\Program Files\obs-studio\obs-plugins\64bit\`
   - `data/obs-plugins/BitrateSceneSwitch/` → `C:\Program Files\obs-studio\data\obs-plugins\`
4. Restart OBS Studio

### macOS / Linux
Coming soon!

## Usage

1. Open OBS Studio
2. Go to **Tools** → **Bitrate Scene Switch**
3. Configure your settings:
   - Enable automatic scene switching
   - Add your stream server(s) with the stats URL
   - Set your bitrate thresholds
   - Select your Normal, Low, and Offline scenes
4. Click **Save**

## Configuration

### Stream Servers

| Server Type | Stats URL Example | Notes |
|-------------|-------------------|-------|
| BELABOX | `https://cloud.belabox.net/stats` | Use your BELABOX cloud stats endpoint |
| NGINX RTMP | `http://localhost:8080/stat` | Requires nginx-rtmp-module with stats enabled |
| SRT Live Server | `http://localhost:8181/stats` | SLS stats API endpoint |
| MediaMTX | `http://localhost:9997/v3/paths/get/` | MediaMTX API endpoint |

### Trigger Thresholds

- **Low Bitrate Threshold**: Scene switches to "Low" when bitrate drops below this value (default: 800 kbps)
- **RTT Threshold**: For SRT streams, switch to "Low" when RTT exceeds this value (default: 2500 ms)
- **Offline Threshold**: Switch to "Offline" when bitrate drops below this value (0 = use server offline detection)

### Retry Attempts

The plugin checks bitrate every second. The retry attempts setting determines how many consecutive checks must show the same state before switching scenes. This prevents rapid scene switching due to momentary bitrate fluctuations.

## Building from Source

### Requirements
- CMake 3.16+
- Qt6
- OBS Studio source/headers
- CURL library

### Windows
```powershell
cmake -B build -S .
cmake --build build --config Release
```

### macOS/Linux
```bash
cmake -B build -S .
cmake --build build
```

## Credits

- Based on [NOALBS](https://github.com/NOALBS/nginx-obs-automatic-low-bitrate-switching) by the NOALBS team
- Built for the IRL streaming community

## License

This project is licensed under the GPL-2.0 License - see the [LICENSE](LICENSE) file for details.
