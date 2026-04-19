<p align="center">
  <img src="images/banner.svg" alt="BitrateSceneSwitch Banner" width="100%">
</p>

<p align="center">
  <a href="https://github.com/sniffingpickles/BitrateSceneSwitch/releases"><img src="https://img.shields.io/github/v/release/sniffingpickles/BitrateSceneSwitch?style=flat-square" alt="Release"></a>
  <a href="https://github.com/sniffingpickles/BitrateSceneSwitch/actions"><img src="https://img.shields.io/github/actions/workflow/status/sniffingpickles/BitrateSceneSwitch/build.yml?style=flat-square" alt="Build"></a>
  <a href="https://github.com/sniffingpickles/BitrateSceneSwitch/blob/main/LICENSE"><img src="https://img.shields.io/github/license/sniffingpickles/BitrateSceneSwitch?style=flat-square" alt="License"></a>
</p>

<p align="center">
  <i>Inspired by <a href="https://github.com/NOALBS/nginx-obs-automatic-low-bitrate-switching">NOALBS</a>. Big thanks to that crew for paving the road.</i>
</p>

---

<img src="images/header-features.svg" alt="Features" width="100%">

- **Auto scene switching** based on bitrate and SRT RTT
- **Multiple stream servers** with priority and fail-over
- **Twitch and Kick chat** commands with fuzzy scene matching
- **End stream on raid** for Twitch and Kick
- **Custom command names** and chat message templates
- **obs-websocket vendor API** for bots, decks, and overlays
- **RIST stale-frame fix** to clear the frozen last frame
- Optional starting / ending / privacy / refresh scenes

---

<img src="images/header-screenshots.svg" alt="Screenshots" width="100%">

<table>
  <tr>
    <td><img src="images/screenshots/general-tab.png" alt="General"></td>
    <td><img src="images/screenshots/triggers-tab.png" alt="Triggers"></td>
    <td><img src="images/screenshots/scenes-tab.png" alt="Scenes"></td>
  </tr>
  <tr>
    <td><img src="images/screenshots/servers-tab.png" alt="Servers"></td>
    <td><img src="images/screenshots/chat-twitch-tab.png" alt="Chat (Twitch)"></td>
    <td><img src="images/screenshots/chat-kick-tab.png" alt="Chat (Kick)"></td>
  </tr>
  <tr>
    <td><img src="images/screenshots/commands-tab.png" alt="Commands"></td>
    <td><img src="images/screenshots/messages-tab.png" alt="Messages"></td>
    <td><img src="images/screenshots/advanced-settings-tab.png" alt="Advanced"></td>
  </tr>
</table>

---

<img src="images/header-installation.svg" alt="Installation" width="100%">

1. Grab the latest build from [**Releases**](https://github.com/sniffingpickles/BitrateSceneSwitch/releases)
2. Extract the zip
3. Drop `BitrateSceneSwitch.dll` into `C:\Program Files\obs-studio\obs-plugins\64bit\`
4. Restart OBS

> Windows x64 and Windows ARM64 builds are published per release.

---

<img src="images/header-usage.svg" alt="Usage" width="100%">

1. **Tools → Bitrate Scene Switch**
2. Add your stream server on the **Servers** tab (IRLhosting, BELABOX, NGINX, SRT, RIST, etc.)
3. Pick your **Live / Low / Offline** scenes on the **Scenes** tab
4. Tune your bitrate and RTT thresholds on the **Triggers** tab
5. Hit **Save**

The status bar at the top of the dialog shows live bitrate, RTT, and the active scene so you can verify it's wired up before going live.

---

<img src="images/header-chat-commands.svg" alt="Chat Commands" width="100%">

Works with **Twitch** and **Kick** chat. By default, only the broadcaster can use commands; add other allowed users on the Chat tab. Every command name can be renamed under the **Commands** tab.

| Command | What it does |
|---------|--------------|
| `!live` | Switch to your Live scene |
| `!low`  | Switch to your Low scene |
| `!brb`  | Switch to your BRB / Offline scene |
| `!privacy` | Switch to your Privacy scene |
| `!ss <name>` | Switch to any scene by name. Fuzzy match, so `!ss priv` will hit `PRIVACY` |
| `!s <name>` | Same as `!ss`, shorter alias |
| `!refresh` | Re-trigger the current scene (clears stuck sources) |
| `!fix` | Refresh media sources (RTMP, SRT, RIST, etc.) |
| `!status` | Reply with current bitrate, RTT, and scene |
| `!trigger` | Force a switch check immediately |
| `!start` | Start streaming |
| `!stop`  | Stop streaming |

---

<img src="images/header-features.svg" alt="Raid Stop" width="100%">

### End stream on raid

When you raid out (Twitch raid or Kick host), the plugin can stop the stream automatically so you don't keep encoding into a dead chat. Toggle it on the **Chat** tab.

- **Twitch**: listens via PubSub for `raid.<your_user_id>` and reacts to both the timer running out and the "Raid Now" button.
- **Kick**: listens to the chatroom Pusher channel for `ChatMoveToSupportedChannelEvent`.
- Optional chat announcement before the stream stops (Twitch only).

---

<img src="images/header-websocket.svg" alt="WebSocket API" width="100%">

BitrateSceneSwitch registers as an **obs-websocket vendor**, so bots, Stream Decks, and overlays can drive the plugin remotely. Requires [obs-websocket](https://github.com/obsproject/obs-websocket) (bundled with OBS 28+).

All requests use vendor name `BitrateSceneSwitch` via the obs-websocket `CallVendorRequest` op.

### Requests

| Request | Description | Parameters | Response |
|---------|-------------|------------|----------|
| `GetSettings` | Get all plugin settings | _none_ | `enabled`, `onlyWhenStreaming`, `instantRecover`, `retryAttempts`, triggers, scenes |
| `SetSettings` | Update settings (partial updates supported) | Any settings field (e.g. `enabled`, `triggerLow`, `sceneNormal`) | `success: true` |
| `GetStatus` | Live status | _none_ | `currentScene`, `isStreaming`, `bitrateKbps`, `rttMs`, `isOnline`, `serverName`, `statusMessage`, `enabled` |
| `SwitchScene` | Switch to a specific scene | `sceneName` (string, required) | `success`, `error` if failed |
| `StartStream` | Start streaming | _none_ | `success`, `error` if already streaming |
| `StopStream` | Stop streaming | _none_ | `success`, `error` if not streaming |
| `GetVersion` | Plugin version | _none_ | `version`, `vendor` |

### Example (raw obs-websocket JSON)

```json
{
  "op": 6,
  "d": {
    "requestType": "CallVendorRequest",
    "requestId": "1",
    "requestData": {
      "vendorName": "BitrateSceneSwitch",
      "requestType": "GetStatus"
    }
  }
}
```

### Example (obs-websocket-js)

```js
const resp = await obs.callVendorRequest("BitrateSceneSwitch", "GetStatus", {});
console.log(resp.bitrateKbps, resp.rttMs, resp.currentScene);
```

---

<img src="images/header-servers.svg" alt="Supported Servers" width="100%">

Add as many servers as you want on the **Servers** tab. Each one has its own priority; the plugin uses the highest-priority server that's reporting bytes.

| Server | Stats URL Example |
|--------|-------------------|
| **BELABOX** | `https://cloud.belabox.net/stats` |
| **NGINX RTMP** | `http://localhost:8080/stat` |
| **[Moo RIST](https://github.com/moo-the-cow/moo-rist-hosting-native)** | `http://localhost:8080/stats` |
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

CI builds on Windows x64 and Windows ARM64. macOS builds locally for development.

---

<img src="images/header-license.svg" alt="License" width="100%">

GPL-2.0

---

<p align="center">
  <sub>v1.0.8 | Powered by <a href="https://irlhosting.com"><b>IRL</b><span style="color: #22d3ee;">Hosting</span></a></sub>
</p>
