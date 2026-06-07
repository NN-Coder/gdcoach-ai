1. AI Analysis & Context Architecture

Context Priority: Latest Attempt → Current Session → Long-Term History. Focus on immediate flaws; use history only for macro progression.

Session Definition: Starts when entering a level, ends when exiting to menus or closing the game.

Session Reset: Chat logs wipe completely blank at the start of every new session.

Level Isolation: Every level retains its own separate history, metadata, and chat logs.

The 5% Filter: Ignore all telemetry for attempts ending at 5% or lower on Medium Demon levels or below.

2. Storage & Memory Optimization

Attempt Data: Stores full telemetry (input frames, coordinates, velocity) for the latest attempt only.

Post-Attempt Cleanup: When a new attempt starts, delete the previous raw telemetry and replace it with a one-line summary (e.g., Attempt #34: Died at 64% on a Triple Spike).

Post-Session Compression: When exiting a level, delete all individual attempt summaries and compile them into a single, permanent Session Summary.

Metadata Separation: Store static level data (Name, ID, Difficulty, Hitbox maps) in a separate database schema once the coach is first used. The AI references this base block to save token space.



## Some more info on the features:
Priority of context: attempt, session, history. Session is defined by when a player starts a level, all attempts on that level until the players exits the level or the game is closed. The Ai should focus on the attempt and session more and only use history for long-term progression analysis. 

The chat should reset to blank every new session

Each level should have its own data like history and chat logs

In the level storage, data should be stored like such for optimization: the latest attempt has all in-depth data about the attempt, afterwards all attempts in-depth data is deleted and a very short summary of the attempt replaces it (e.g. attempt #34, player died at 64% at triple spike), then after the session is ended all attempt summaries are deleted and a more in-depth session summary is created (if it was a short session then there doesnt need to be much but if long then it can be longer) and stored in the level data permanently and long term history. Also once the player has used ai coach on a level, the overall level data should be stored separately from each attempt data like the level name, id, difficulty, components, maybe hitboxes and timings, etc.) so when the AI does an analysis it looks at the overall level data and then the attempt, session, history data based on priority as discussed before