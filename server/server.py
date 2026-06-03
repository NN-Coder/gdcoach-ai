"""
GDCoach AI — Inference Server (Gemini edition)
===============================================
FastAPI server that bridges the Geode mod and Google Gemini 2.5 Flash.

Routes
------
POST /analyze  — Accepts a JSON telemetry payload, builds a structured
                 coaching prompt, calls Gemini, and returns plain-text
                 coaching advice.
GET  /health   — Lightweight liveness probe.

Run
---
    uvicorn server:app --host 0.0.0.0 --port 8000 --reload

Environment variables (see .env.example)
-----------------------------------------
    GEMINI_API_KEY  — Required. Your Google AI Studio API key.
    GEMINI_MODEL    — Optional. Defaults to gemini-2.5-flash.
    MAX_TOKENS      — Optional. Max output tokens. Defaults to 300.

SDK note: uses the current `google-genai` package (google.genai),
not the deprecated `google-generativeai` package.
"""

from __future__ import annotations

import os
from collections import Counter

from google import genai
from google.genai import types as genai_types
from dotenv import load_dotenv
from fastapi import FastAPI, HTTPException
from fastapi.responses import PlainTextResponse
from pydantic import BaseModel, Field

# ── Environment ───────────────────────────────────────────────────────────────

load_dotenv()

GEMINI_API_KEY = os.getenv("GEMINI_API_KEY", "")
GEMINI_MODEL   = os.getenv("GEMINI_MODEL", "gemini-2.5-flash")
MAX_TOKENS     = int(os.getenv("MAX_TOKENS", "300"))

if not GEMINI_API_KEY:
    raise RuntimeError(
        "GEMINI_API_KEY is not set. "
        "Copy .env.example → .env and fill in your key."
    )

# Instantiate the google-genai client
_client = genai.Client(api_key=GEMINI_API_KEY)

# ── Pydantic models ───────────────────────────────────────────────────────────

class LevelInfo(BaseModel):
    name:          str  = Field(default="Unknown Level")
    creator:       str  = Field(default="Unknown")
    difficulty:    int  = Field(default=0, ge=0, le=10)
    level_id:      int  = Field(default=0)
    is_platformer: bool = Field(default=False)


class DeathRecord(BaseModel):
    percentage:     float = Field(..., ge=0.0, le=100.0)
    gamemode:       str   = Field(...)
    attempt_number: int   = Field(..., ge=1)


class TelemetryPayload(BaseModel):
    level:            LevelInfo
    deaths:           list[DeathRecord] = Field(default_factory=list)
    attempt_count:    int               = Field(default=0, ge=0)
    current_gamemode: str               = Field(default="Cube")


# ── App ───────────────────────────────────────────────────────────────────────

app = FastAPI(
    title="GDCoach AI",
    description="AI coaching inference server for the GDCoach Geode mod (Gemini edition).",
    version="0.1.0",
)

# ── Prompt engineering helpers ────────────────────────────────────────────────

_DIFFICULTY_MAP = {
    0: "N/A",
    1: "Auto",
    2: "Easy",
    3: "Normal",
    4: "Hard",
    5: "Harder",
    6: "Insane",
    7: "Easy Demon",
    8: "Medium Demon",
    9: "Hard Demon",
    10: "Insane Demon",
}

_SYSTEM_PROMPT = (
    "You are GDCoach, an expert Geometry Dash coach with deep knowledge of "
    "every gamemode, level design, and practice technique. "
    "You will receive structured data about a player's session. "
    "Your response MUST be plain text with NO markdown, NO bullet points, "
    "NO headers, and NO special characters. "
    "Write exactly three short paragraphs separated by a blank line:\n"
    "  1. Death pattern analysis — where the player is dying and why.\n"
    "  2. Gamemode skill gap — which vehicle skill is the bottleneck "
    "(e.g., wave spam timing, ship straight-fly, UFO click rhythm).\n"
    "  3. Practice recommendations — name 2-3 specific Geometry Dash "
    "practice levels or creators that target the identified weakness.\n"
    "Keep the total response under 180 words. Be specific, direct, and "
    "encouraging. Never say 'I' or refer to yourself."
)


def _find_death_cluster(percentages: list[float], window: float = 10.0) -> float:
    """Return the midpoint of the 10%-wide window with the most deaths."""
    if not percentages:
        return 0.0
    best_start = 0.0
    best_count = 0
    for pct in percentages:
        count = sum(1 for p in percentages if pct <= p < pct + window)
        if count > best_count:
            best_count = count
            best_start = pct
    return best_start + window / 2.0


def _build_user_message(payload: TelemetryPayload) -> str:
    """Summarise the telemetry into a compact natural-language prompt."""
    level  = payload.level
    deaths = payload.deaths

    difficulty_label = _DIFFICULTY_MAP.get(level.difficulty, "Unknown")
    level_type       = "platformer" if level.is_platformer else "classic"

    if deaths:
        percentages     = [d.percentage for d in deaths]
        avg_pct         = sum(percentages) / len(percentages)
        furthest_pct    = max(percentages)
        gamemode_counts = Counter(d.gamemode for d in deaths)
        most_deadly_gm  = gamemode_counts.most_common(1)[0][0]
        cluster_pct     = _find_death_cluster(percentages)

        death_summary = (
            f"The player has died {len(deaths)} times across "
            f"{payload.attempt_count} attempts. "
            f"Average death at {avg_pct:.1f}%, furthest run at {furthest_pct:.1f}%. "
            f"Most deaths occurred as {most_deadly_gm}. "
            f"The densest death cluster is around {cluster_pct:.1f}%."
        )
    else:
        death_summary = "No deaths recorded in this session yet."

    return (
        f"Level: '{level.name}' by {level.creator} "
        f"(ID: {level.level_id}, {difficulty_label} {level_type}).\n"
        f"{death_summary}\n"
        f"Current active gamemode at pause: {payload.current_gamemode}."
    )


# ── Routes ────────────────────────────────────────────────────────────────────

@app.get("/health")
async def health_check():
    return {"status": "ok", "model": GEMINI_MODEL}


@app.post("/analyze", response_class=PlainTextResponse)
async def analyze(payload: TelemetryPayload) -> str:
    """
    Accepts a TelemetryPayload, builds a coaching prompt, calls Gemini 2.5 Flash,
    and returns plain-text advice.
    """
    user_message = _build_user_message(payload)

    # Combine system prompt and user message — Gemini's generate_content
    # supports a system_instruction field for the system prompt.
    try:
        response = _client.models.generate_content(
            model=GEMINI_MODEL,
            contents=user_message,
            config=genai_types.GenerateContentConfig(
                system_instruction=_SYSTEM_PROMPT,
                max_output_tokens=MAX_TOKENS,
                temperature=0.7,
            ),
        )
        advice = response.text or ""
        return advice.strip()

    except Exception as exc:
        raise HTTPException(
            status_code=502,
            detail=f"Gemini request failed: {exc}",
        ) from exc
