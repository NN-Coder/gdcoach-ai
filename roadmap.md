# UI Integration & Layout
* Hook into PauseLayer::customSetup to modify the game's default pause menu arrangement.

* Implement a custom sprite button (ButtonSprite) using the Geode UI design language.

* Create a dedicated custom pop-up sub-layer (FLAlertLayer subclass) to act as the primary AI interaction panel.

* Ensure the entire dashboard handles simple text-wrapping perfectly to display long strings of advice clearly.

# Client-Side Data Collection
* Set up simple tracking listeners for PlayerObject::playerDestroyed to log the exact percentage of every death.

* Program tracking variables to read the active player state (Cube, Ship, Ball, UFO, Wave, Robot, Spider, Swingcopter).

* Map current level metadata (Name, Creator, Difficulty) into a temporary memory struct whenever a level loads.

# API Bridge & Prompt Processing
* Set up a clean, single-file server script to format input data into an system prompt.

* Configure an asynchronous web::WebRequest cycle within Geode so that checking the coach never hangs or drops the main thread's frame rate.

* Write a concise system prompt template that outputs specific, plain-text sentences containing the timing assessments, skill requirements, and level recommendations.

# Optimization & Anti-Cheat
* Add a configuration setting to disable the button completely when attempting formal Demon List runs, ensuring clean, unmodified recording files.

* Limit network payload flushes strictly to the initialization frame of the pause menu click to avoid background data overhead during live gameplay.

# Definition of Success
* A successful MVP will establish a lightweight, asynchronous bridge between Geometry Dash (via the Geode framework) and an external AI inference server, triggered exclusively from a custom button injected into PauseLayer::customSetup.

* The client-side success is defined by its ability to silently cache local telemetry data—specifically level metadata from loading states, active gamemode states from PlayerObject, and death coordinates/percentages via PlayerObject::playerDestroyed—without impacting the game's main thread or frame rate. Upon pausing and clicking the custom ButtonSprite, the client must compile this structured struct data into a single, non-blocking web::WebRequest payload.

* The server-side success requires a dedicated script to parse this telemetry, inject it into a highly constrained system prompt, and return a clean, plain-text analysis detailing input timing errors, specific vehicle skill bottlenecks (e.g., wave spamming, ship control), and targeted practice level recommendations.

* Finally, the MVP must elegantly render this string using standard text-wrapping within a custom FLAlertLayer sub-panel, while guaranteeing competitive integrity by including a toggle config to completely strip the UI hook during official Demon List attempts.