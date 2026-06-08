# GD Coach AI - Project Roadmap

This document outlines the architectural plan, feature set, and integration steps for the Geometry Dash AI Coach (GDCoach) Geode mod. The project bridges client-side C++ gameplay monitoring with server-side AI model inference to provide players with real-time coaching advice.

---

## 1. UI Integration & Text Input Layout

Enhance the user interface within Geometry Dash to support rich, interactive chat capabilities and data control without UI glitching or clipping.

* [INPROGRESS] **Pause Menu Injection**: Hook into [PauseLayer::customSetup](file:///c:/Users/Neil/Documents/GitHub/gdcoach-ai/src/main.cpp#L113) to inject the "AI Coach" button ([ButtonSprite](file:///c:/Users/Neil/Documents/GitHub/gdcoach-ai/src/main.cpp#L129)), respecting competitive integrity settings (disable in official runs).
* [DONE] **AI Panel Subclass**: Leverage a custom [FLAlertLayer](file:///c:/Users/Neil/Documents/GitHub/gdcoach-ai/src/CoachLayer.hpp#L27) subclass in [CoachLayer](file:///c:/Users/Neil/Documents/GitHub/gdcoach-ai/src/CoachLayer.hpp#L29) to host the interaction workspace.
* **ASCII Keyboard Listener**: Implement an ASCII character conversion listener to intercept native keyboard events in the Cocos2d-x environment, enabling typing of standard keyboard symbols (`?`, `/`, `!`, `@`, etc.) that are natively rendered inside Geometry Dash.
* **Dynamic Input Wrapping**: Program the chat text input box to dynamically shrink its font size and expand vertically into multi-line layouts as players type longer queries.
* [DONE] **Enter-To-Send Binding**: Map the keyboard `Enter` key to immediately dispatch the chat payload.
* [DONE] **Right-Edge Clipping Fix**: Correct bounding box layout constraints to stop dynamic texts from cutting off at the right side of the screen on non-standard resolutions.
* [DONE] **Scroll Position Lock**: Modify the [ScrollLayer](file:///c:/Users/Neil/Documents/GitHub/gdcoach-ai/src/CoachLayer.hpp#L6) behavior to lock the scroll position, preventing the chat container from automatically jumping back to the top when sending or receiving messages.
* [DONE] **Clean Text Rendering**: Ensure accurate Cocos2d-x font scaling, margins, and vertical alignment for all dynamic chat bubbles and status logs.

---

## 2. Client-Side Data Collection & Physics Calibration

Accrue gameplay telemetry data from live sessions to provide accurate inputs for AI analysis.

* **Death Events**: Hook [PlayerObject::playerDestroyed](file:///c:/Users/Neil/Documents/GitHub/gdcoach-ai/src/main.cpp#L212) (via `destroyPlayer` inside [PlayLayer](file:///c:/Users/Neil/Documents/GitHub/gdcoach-ai/src/main.cpp#L184)) to record exact percentage metrics.
* **Vehicle Tracking**: Inspect player gamemode states (Cube, Ship, Ball, UFO, Wave, Robot, Spider, Swing) inside [PlayerObject::update](file:///c:/Users/Neil/Documents/GitHub/gdcoach-ai/src/main.cpp#L234) to track active forms.
* **Frame-Perfect Physics Calibration**: Use the C++ backend to compute native physics frame boundaries and log hyper-accurate collision telemetry (e.g., exact frame offsets of death and hitbox overlaps).
* [DONE] **The 5% Filter**: Programmatically ignore telemetry logs for attempts ending at 5% or lower on Medium Demon levels or below to avoid cluttering histories with instant restarts.

---

## 3. AI Analysis & Context Architecture

Govern how the AI consumes attempt histories, session boundaries, and pre-programmed information.

* [DONE] **Context Priority Stack**: Structure the prompt engine to prioritize context in this order: **Attempt ≈ Session > History**. Focus primarily on the current Attempt metrics and the current Session trends equally. Use the long-term History purely for macro-progression tracking and overarching trends across sessions.
* [DONE] **Session Definition**: Define a session as starting when entering a level, and ending when exiting back to menus or closing the game.
* [DONE] **Session Reset**: Wipe the active chat logs completely blank at the start of every new session.
* [DONE] **Level Isolation**: Maintain level-specific sandboxes; every level retains its own separate history, static metadata database, and chat logs.
* **RobTop Pre-Training**: Pre-program exact collision hitboxes, frame timings, and percentage boundaries for all 22 official RobTop levels directly into the system prompt to feed the AI precise level details.

---

## 4. Storage & Memory Optimization

Reduce disk footprint and token waste through data lifecycle management.

* [DONE] **Global Storage Architecture**: All level data is saved within `neil.gdcoach-ai/levels/`. Official levels use the naming scheme `levels/{level_name}`, while custom levels use `levels/{level_name}_{level_id}`.
* [INPROGRESS] **Attempt Telemetry Buffering**: Retain detailed high-frequency telemetry (input frame arrays, coordinates, velocity) for the **latest attempt only**.
* [DONE] **Post-Attempt Cleanup**: When a new attempt begins, delete the previous attempt's raw telemetry and replace it with a lightweight one-line summary (e.g., *Attempt #34: Died at 64% on a Triple Spike*).
* [DONE] **Post-Session Compression**: Upon exiting a level, wipe all individual one-line attempt summaries and compile them into a unified, permanent **Session Summary** stored in [TelemetryManager](file:///c:/Users/Neil/Documents/GitHub/gdcoach-ai/src/TelemetryManager.hpp#L87)'s local level `history.json` database.
* [DONE] **Metadata Separation Schema**: Write static level details (Name, ID, Difficulty, Hitbox maps) to a separate `metadata.json` schema once a level is loaded for the first time. The AI references this base block to avoid repeating static details in every request.
* **Video Triage (Alternative Buffer)**: Maintain a local video buffer of the latest attempt only, overwriting it immediately on the next attempt or after AI analysis finishes.

---

## 5. Backend, Models & Token Optimization

Configurable inference routing and budget management.

* **Inference Routing**: Introduce a setting toggle to switch execution between **Gemini API (Cloud)** and **Ollama (Local)**.
* **Model Selection Dropdown**: Add a configuration dropdown to select specific model architectures (e.g., `gemini-2.0-flash` vs. local quantized GGUF models) in general and for specific circumstances like different models for analysis, chat, session summaries, etc.  
* **On-Demand Processing**: Deactivate automatic analysis loops. Web requests trigger only when the user manually clicks the UI button.
* [DONE] **Token Limit Control**: Add an adjustable slider bar in Geode settings to modify maximum output limits. The backend reads this value dynamically for the next inference call.
* [DONE] **Token Output Awareness**: Implement systemic prompting and tracking to ensure the AI monitors output token limits and finishes its sentences cleanly before running out of generation space.

---

## 6. Data Management Screen

Give players full transparency and control over their local coaching data.

* [INPROGRESS] **Global Dashboard**: Integrate a custom interface ([DeleteDataLayer](file:///c:/Users/Neil/Documents/GitHub/gdcoach-ai/src/DeleteDataLayer.hpp#L21)) displaying a breakdown of all saved data categories (telemetry, session summaries, histories) and their disk usage in KB/MB.
* [INPROGRESS] **Targeted Deletion**: Support clearing specific categories of data (e.g., wipe chat histories but preserve session progression statistics).
* [INPROGRESS] **In-Context Management**: Enable opening the level data dashboard directly from within the pause menu of a level to quickly manage storage for that level.

---

## 7. Specialized Features & Content

Differentiate coach feedback style and support targeted coaching objectives.

* **Coach Personalities Setting**: Toggle between:
  - *Encouraging*: Emphasizes milestones, motivational support, and burnout prevention.
  - *Realistic*: Emphasizes raw timing metrics, exact frame errors, and explicit skill flaws.
* **Specialized System Prompts & Guides**: Modular prompts and UI activated based on user requests:
  - *Achievements*: Focuses on icon unlock paths and optimization.
  - *Coins*: Details paths, key timings, and triggers for hidden coins.
  - *Gamemodes*: Isomorphic training routines to isolate specific vehicle forms (e.g., Wave spamming, dual control splits).
  - *Progression*: Recommends online community levels matched to the player's current skill profile.
* **Custom AI Level Generation**: Use AI to create customized levels for the user based on their needs (e.g., to help improve in specific gamemodes, create a fun tailored experience, or design a progression curve).
* **AI Training Packs**: Create series of levels grouped into packs that guide players along a specific improvement trajectory over time.

---

## 8. Definition of Success

* **Integration Integrity**: A successful deployment establishes a lag-free bridge between Geometry Dash (via the Geode framework) and the cloud/local inference backend, utilizing the custom button injected into [PauseLayer::customSetup](file:///c:/Users/Neil/Documents/GitHub/gdcoach-ai/src/main.cpp#L113).
* **Telemetry & Optimization**: The client correctly buffers high-frequency telemetry for the latest attempt only, replacing it with a one-line summary on reset and compressing it into a permanent Session Summary on level exit, maintaining zero frame drops or lag spikes during active play.
* **Robust Text UI**: A custom-wrapped [FLAlertLayer](file:///c:/Users/Neil/Documents/GitHub/gdcoach-ai/src/CoachLayer.hpp#L27) displaying fully wrapped text, working scroll position lock, full ASCII keyboard input support, dynamic vertical font scaling, and zero right-side clipping.
* **Data Control**: The [DeleteDataLayer](file:///c:/Users/Neil/Documents/GitHub/gdcoach-ai/src/DeleteDataLayer.hpp#L21) global dashboard lets players inspect exact storage footprints on disk and selectively delete logs globally or for the active level in-context.
* **Flexible Inference**: The backend correctly routes queries to cloud/local models, honors token boundaries to finish sentences cleanly, and applies personality presets dynamically.