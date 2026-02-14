# Bitrate Scene Switch

An OBS Studio plugin for automatic scene switching based on stream server bitrate.

![Plugin Banner](screenshots/banner.png)

## Features

- Automatic scene switching based on bitrate thresholds
- Support for multiple stream server types
- RTT monitoring for SRT streams
- Configurable triggers and retry logic
- Optional starting/ending/privacy scenes

## Screenshots

![Settings Dialog](screenshots/settings.png)

![Server Configuration](screenshots/servers.png)

## Installation

### Windows

1. Download the latest release from [Releases](https://github.com/sniffingpickles/BitrateSceneSwitch/releases)
2. Extract the zip file
3. Copy contents to your OBS installation:
   - `obs-plugins/64bit/BitrateSceneSwitch.dll` → `C:\Program Files\obs-studio\obs-plugins\64bit\`
   - `data/obs-plugins/BitrateSceneSwitch/` → `C:\Program Files\obs-studio\data\obs-plugins\`
4. Restart OBS Studio

## Usage

1. Open OBS Studio
2. Go to **Tools** → **Bitrate Scene Switch**
3. Configure your stream server and thresholds
4. Select your scenes and click **Save**

## Supported Servers

| Server | Stats URL Example |
|--------|-------------------|
| BELABOX | `https://cloud.belabox.net/stats` |
| NGINX RTMP | `http://localhost:8080/stat` |
| SRT Live Server | `http://localhost:8181/stats` |
| MediaMTX | `http://localhost:9997/v3/paths/get/` |
| Node Media Server | `http://localhost:8000/api/streams` |
| Nimble | `http://localhost:8082` |

## Building

```bash
cmake -B build -S .
cmake --build build --config Release
```

## License

GPL-2.0
