# GDCoach AI

> Personalized AI coaching for Geometry Dash, delivered as a Geode mod.

GDCoach AI silently tracks your session — level metadata, active gamemode, and every death percentage — then sends that telemetry to a local AI inference server when you click **AI Coach** in the pause menu. The server returns targeted, plain-text advice on your death patterns, gamemode skill gaps, and practice level recommendations.

---

## Architecture

```
Geometry Dash (Geode mod)
    │
    ├─ PauseLayer hook     → injects "AI Coach" button
    ├─ PlayLayer hook      → captures level info + death %
    └─ PlayerObject hook   → tracks active gamemode per frame
            │
            │  async POST /analyze (JSON telemetry)
            ▼
    GDCoach Inference Server (FastAPI + Python)
            │
            │  OpenAI / compatible LLM call
            ▼
    Plain-text coaching advice
            │
            ▼
    CoachLayer popup (FLAlertLayer-style, word-wrapped)
```

---

## Geode Mod — Build

### Prerequisites
- [Geode CLI](https://geode-sdk.org/install/) installed and `GEODE_SDK` environment variable set
- CMake ≥ 3.21
- MSVC 2022 (Windows) or Clang (macOS)

### Build

```bash
cmake -B build
cmake --build build
```

The `.geode` package will be emitted to `build/` and auto-installed if the CLI is configured.

---

## Inference Server — Setup

### Prerequisites
- Python ≥ 3.11
- An OpenAI API key (or any OpenAI-compatible provider)

### Install

```bash
cd server
pip install -r requirements.txt
cp .env.example .env
# Edit .env and add your OPENAI_API_KEY
```

### Run

```bash
uvicorn server:app --host 0.0.0.0 --port 8000 --reload
```

The server defaults to `http://localhost:8000`. To use a different address, update the **Inference Server URL** setting in the Geode mod settings panel.

---

## Settings

| Setting | Default | Description |
|---|---|---|
| **Demon List Mode** | `false` | Hides the AI Coach button completely during official Demon List attempts |
| **Inference Server URL** | `http://localhost:8000` | Base URL of your GDCoach inference server |

---

## Roadmap

See [roadmap.md](roadmap.md) for the full MVP specification.

---

## License

MIT — see [LICENSE](LICENSE).
