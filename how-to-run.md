# How to Run GDCoach AI

This guide covers every step needed to see the mod live inside Geometry Dash.
Steps marked **[Manual]** require action from you — they involve installing software, building the mod, or interacting with the game itself.
Steps marked **[Done]** are already handled by the code in this repo.

---

## Part 1 — Prerequisites

### [Manual] 1.1 Install Geometry Dash
Make sure Geometry Dash is installed via Steam and has been launched at least once.
The expected path is:
```
C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\GeometryDash.exe
```

### [Manual] 1.2 Install the Geode Mod Loader
Geode is the mod framework this project uses. Without it, the mod cannot load.

1. Go to **https://geode-sdk.org/install/**
2. Download the **Windows installer** (`GeodeInstaller.exe`)
3. Run the installer and point it at your GD executable above
4. Launch GD — a Geode icon should now appear in the bottom-right corner of the main menu

### [Manual] 1.3 Install the Geode CLI
The CLI is needed to build the mod from source.

```powershell
winget install GeodeSDK.GeodeCLI
```
> If `winget` is unavailable, download from: https://github.com/geode-sdk/cli/releases

Verify:
```powershell
geode --version
```

### [Manual] 1.4 Install the Geode SDK
```powershell
geode sdk install
```
This installs the SDK to `%LOCALAPPDATA%\GeodeSDK` and sets the `GEODE_SDK` environment variable automatically.

Verify:
```powershell
echo $Env:GEODE_SDK
```
You should see a path like `C:\Users\Neil\AppData\Local\GeodeSDK`.

### [Manual] 1.5 Install CMake
If you don't already have it:
```powershell
winget install Kitware.CMake
```
Minimum version: **3.21**

### [Manual] 1.6 Install Visual Studio 2022
Required for the MSVC compiler that Geode uses on Windows.

- Download from: https://visualstudio.microsoft.com/
- During install, select the workload: **"Desktop development with C++"**

### [Manual] 1.7 Enable billing on your Google Cloud project

The Gemini API key requires billing to be enabled on its Google Cloud project before it can make generation calls.

1. Go to **https://console.cloud.google.com/billing** and sign in
2. Select your project: **`projects/862737094774`**
3. Link a billing account (a free trial credit is available)
4. Then go to **https://aistudio.google.com/app/apikey** and confirm your key is active

> **Why?** The free-tier quota for Gemini 2.x models is set to 0 unless billing is enabled. Once billing is linked, you get 15 RPM and 1500 requests/day at no charge for `gemini-2.5-flash`.

---

## Part 2 — Build the Mod

### [Manual] 2.1 Configure CMake

Open a **Developer PowerShell for VS 2022** (or any terminal with MSVC in PATH) and run:

```powershell
cd c:\Users\Neil\Documents\GitHub\gdcoach-ai
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

### [Manual] 2.2 Build

```powershell
cmake --build build --config RelWithDebInfo
```

A successful build produces a file like:
```
build\neil.gdcoach-ai.geode
```

### [Manual] 2.3 Install the Mod into GD

The Geode CLI can install the `.geode` file directly:
```powershell
geode package install build\neil.gdcoach-ai.geode
```

Alternatively, copy the `.geode` file to your Geode mods folder manually:
```
%LOCALAPPDATA%\GeodeData\mods\
```

---

## Part 3 — Start the AI Server

> **These steps can be run from a normal PowerShell window — no C++ tools needed.**

### [Done] 3.1 Dependencies are already installed
The Python packages were installed automatically. If you ever need to reinstall:
```powershell
cd c:\Users\Neil\Documents\GitHub\gdcoach-ai\server
pip install -r requirements.txt
```

### [Done] 3.2 API key is already configured
`server/.env` contains your Gemini API key. This file is gitignored and will never be committed.

### [Manual] 3.3 Start the server

Open a terminal and run:
```powershell
cd c:\Users\Neil\Documents\GitHub\gdcoach-ai\server
uvicorn server:app --host 127.0.0.1 --port 8000 --reload
```

You should see:
```
INFO:     Uvicorn running on http://127.0.0.1:8000 (Press CTRL+C to quit)
```

Keep this terminal open while playing — **the server must be running for the AI Coach button to work**.

### 3.4 Verify the server is healthy (optional)
Open in a browser or run:
```powershell
Invoke-WebRequest http://127.0.0.1:8000/health | Select-Object -ExpandProperty Content
```
Expected response: `{"status":"ok","model":"gemini-2.5-flash"}`

---

## Part 4 — Play and Use the Mod

### [Manual] 4.1 Launch Geometry Dash
Start GD from Steam. The Geode loader runs automatically.

### [Manual] 4.2 Confirm the mod is loaded
In GD, click the **Geode icon** (bottom-right of main menu) → **Installed** tab.
You should see **GDCoach AI v0.1.0** listed and enabled.

### [Manual] 4.3 Configure mod settings (optional)
In the Geode menu → GDCoach AI → **Settings**:

| Setting | What to set |
|---|---|
| **Inference Server URL** | Leave as `http://localhost:8000` (default) |
| **Demon List Mode** | Leave **off** unless recording a ranked attempt |

### [Manual] 4.4 Get coaching feedback
1. Enter any level from the main menu
2. Play and **die at least once** (so telemetry data is collected)
3. Press **Escape** to pause
4. Press the **AI Coach** button (gold button, injected by the mod)
5. The popup appears with a loading spinner
6. In a few seconds, Gemini returns your personalized coaching advice

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| AI Coach button not visible in pause menu | Mod not loaded | Check Geode installed tab; rebuild if missing |
| AI Coach button not visible (demon list mode) | Setting is on | Disable Demon List Mode in mod settings |
| Popup shows "No session data yet" | Opened pause before dying | Die once, then pause and click the button |
| Popup shows "Server returned error 502" | Gemini API/billing issue | Check `server/.env`; ensure billing is enabled on your GCP project |
| Popup shows "Server returned error 502" (quota) | Free-tier quota at 0 | Enable billing at console.cloud.google.com/billing for project 862737094774 |
| Popup shows "Request was cancelled" | Server not running | Start `uvicorn server:app --port 8000` |
| Build fails: "GEODE_SDK not found" | SDK not installed | Run `geode sdk install` and restart terminal |
| CMake can't find compiler | VS 2022 not installed or wrong terminal | Use "Developer PowerShell for VS 2022" |
