export default {
  async fetch(request, env, ctx) {
    if (request.method === "OPTIONS") {
      return new Response(null, {
        headers: {
          "Access-Control-Allow-Origin": "*",
          "Access-Control-Allow-Methods": "POST, OPTIONS",
          "Access-Control-Allow-Headers": "Content-Type",
        },
      });
    }

    const url = new URL(request.url);
    if (request.method !== "POST" || (url.pathname !== "/analyze" && url.pathname !== "/summarize")) {
      return new Response("Not Found", { status: 404 });
    }

    if (!env.GEMINI_API_KEY) {
      return new Response("Server configuration error: API Key missing.", { status: 500 });
    }

    try {
      const payload = await request.json();

      if (url.pathname === "/summarize") {
        const level = payload.level || {};
        const attempts = payload.attempts || [];
        const chatHistory = payload.chat_history || [];

        if (attempts.length === 0) {
          return new Response("No attempts recorded this session.", {
            status: 200,
            headers: { "Access-Control-Allow-Origin": "*" }
          });
        }

        let contextMsg = `Here is the list of attempts from the play session of level "${level.name || "Unknown"}" by ${level.creator || "Unknown"} (ID: ${level.level_id || 0}):\n`;
        attempts.forEach(s => {
          contextMsg += `- ${s}\n`;
        });

        if (chatHistory.length > 0) {
          contextMsg += `\nHere is the chat history between the player and their AI coach during this session:\n`;
          chatHistory.forEach(msg => {
            contextMsg += `${msg.role === "user" ? "Player" : "Coach"}: ${msg.text}\n`;
          });
        }

        const systemPrompt = `You are GDCoach's Session Summarizer.
Your task is to generate a concise, objective summary of the player's level session.
You will receive a list of attempt summaries (where they died) and the chat logs between the player and their AI coach.

Format the output as a clean, brief summary (1-2 short paragraphs) that:
1. Identifies the furthest progress achieved and the primary choke point(s).
2. Synthesizes any coaching advice or player inquiries from the chat logs.
3. Provides a final encouraging but realistic takeaway for their next session.

Do not include markdown headers, bullet points, or list structures. Write in a concise, human-like voice. Max 150 words.`;

        const geminiRequestBody = {
          systemInstruction: { parts: [{ text: systemPrompt }] },
          contents: [{ role: "user", parts: [{ text: contextMsg }] }],
          generationConfig: {
            temperature: 0.5,
            maxOutputTokens: 500
          }
        };

        const model = "gemini-2.5-flash";
        const apiUrl = `https://generativelanguage.googleapis.com/v1beta/models/${model}:generateContent?key=${env.GEMINI_API_KEY}`;

        const response = await fetch(apiUrl, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(geminiRequestBody)
        });

        const data = await response.json();

        if (!response.ok) {
          console.error("Gemini API Error (Summarize):", JSON.stringify(data));
          return new Response("Error communicating with AI.", {
            status: 502,
            headers: { "Access-Control-Allow-Origin": "*" }
          });
        }

        const text = data.candidates?.[0]?.content?.parts?.[0]?.text || "No summary generated.";

        return new Response(text, {
          headers: { "Content-Type": "text/plain", "Access-Control-Allow-Origin": "*" }
        });
      }

      const difficultyMap = ["N/A","Auto","Easy","Normal","Hard","Harder","Insane","Easy Demon","Medium Demon","Hard Demon","Insane Demon"];
      const level = payload.level || {};
      const difficultyLabel = difficultyMap[level.difficulty] || "Unknown";
      const isOfficial = level.level_id > 0 && level.level_id <= 22; // Main GD official levels

      // ── Token budget from client (slider, default 1024) ────────────────────
      const maxTokens = Math.min(Math.max(Number(payload.max_tokens) || 1024, 256), 4096);

      // ── Filter out <= 1% deaths (player pausing to open menu) ───────────────
      const allDeaths = (payload.deaths || []).filter(d => d.percentage > 1.0);
      const attemptCount = payload.attempt_count || 0;

      // ── Build a structured analysis report ──────────────────────────────────
      let contextMsg = `=== SESSION REPORT ===\n`;
      contextMsg += `Level: "${level.name || "Unknown"}" by ${level.creator || "Unknown"} (ID: ${level.level_id || 0}, Difficulty: ${difficultyLabel}${isOfficial ? ", Official GD Level" : ""}).\n`;
      contextMsg += `Total attempts this session: ${attemptCount}.\n`;

      if (allDeaths.length === 0) {
        contextMsg += `No meaningful deaths recorded yet (deaths at <=1% are ignored as menu-pause artifacts).\n`;
      } else {
        const latest = allDeaths[allDeaths.length - 1];
        contextMsg += `Latest attempt: died at ${latest.percentage.toFixed(1)}% as ${latest.gamemode}.\n`;

        // ── Choke point detection ──────────────────────────────────────────────
        // Find the 5%-window with the most deaths
        let bestStart = 0, bestCount = 0;
        for (const d of allDeaths) {
          const windowStart = Math.floor(d.percentage / 5) * 5;
          const count = allDeaths.filter(x => x.percentage >= windowStart && x.percentage < windowStart + 5).length;
          if (count > bestCount) { bestCount = count; bestStart = windowStart; }
        }
        if (bestCount >= 2) {
          contextMsg += `Choke point: ${bestCount} deaths clustered between ${bestStart.toFixed(0)}% and ${(bestStart + 5).toFixed(0)}%. This is the primary bottleneck.\n`;
        }

        // ── Gamemode breakdown ─────────────────────────────────────────────────
        const gmCounts = {};
        for (const d of allDeaths) {
          gmCounts[d.gamemode] = (gmCounts[d.gamemode] || 0) + 1;
        }
        const gmBreakdown = Object.entries(gmCounts).map(([gm, n]) => `${gm}: ${n} deaths`).join(", ");
        contextMsg += `Deaths by gamemode: ${gmBreakdown}.\n`;

        // ── Progression trend ──────────────────────────────────────────────────
        if (allDeaths.length >= 3) {
          const firstHalf = allDeaths.slice(0, Math.floor(allDeaths.length / 2));
          const secondHalf = allDeaths.slice(Math.floor(allDeaths.length / 2));
          const avgFirst = firstHalf.reduce((s, d) => s + d.percentage, 0) / firstHalf.length;
          const avgSecond = secondHalf.reduce((s, d) => s + d.percentage, 0) / secondHalf.length;
          const improvement = avgSecond - avgFirst;
          if (improvement > 2) {
            contextMsg += `Trend: Player is progressing further (+${improvement.toFixed(1)}% avg improvement over the session).\n`;
          } else if (improvement < -2) {
            contextMsg += `Trend: Player is dying earlier over the session (${improvement.toFixed(1)}% avg), suggesting fatigue or inconsistency.\n`;
          } else {
            contextMsg += `Trend: Runs are consistent but stuck at roughly the same point.\n`;
          }
        }

        // ── Full death log ─────────────────────────────────────────────────────
        contextMsg += `\nAll deaths this session:\n`;
        allDeaths.forEach((d, i) => {
          contextMsg += `  Attempt ${d.attempt_number}: ${d.percentage.toFixed(1)}% as ${d.gamemode}\n`;
        });
      }

      // ── Conversation history ───────────────────────────────────────────────
      const contents = [];
      if (payload.history && payload.history.length > 0) {
        for (const msg of payload.history) {
          contents.push({
            role: msg.role === "user" ? "user" : "model",
            parts: [{ text: msg.text }]
          });
        }
      }

      const latestUserText = payload.message
        ? `${contextMsg}\n\nPlayer message: ${payload.message}`
        : `${contextMsg}\n\nAnalyze this and give me targeted coaching advice.`;

      contents.push({ role: "user", parts: [{ text: latestUserText }] });

      const systemPrompt = `You are GDCoach, a sharp, knowledgeable Geometry Dash coach.

You will receive a structured session report with death percentages, choke points, and game mode data. Your job is to turn that data into precise, actionable coaching.

TOKEN BUDGET: You have a strict limit of approximately ${maxTokens} output tokens. To ensure you do not get cut off, you MUST limit your response to roughly ${Math.floor(maxTokens * 0.6)} words. Always finish on a complete sentence — never cut off mid-word or mid-sentence. If you are running long, wrap up cleanly and concisely.

RULES:
1. Ground every statement in the actual numbers given. Never invent obstacles, spikes, or struggles that aren't in the data.
2. Use your knowledge of the level's layout to pinpoint WHAT is at the death percentage. For official levels (Stereo Madness, Back on Track, Clubstep, etc.) you know the layout — use it. For custom levels, be appropriately vague unless the level is famous (e.g. Bloodbath, Cataclysm).
3. Identify MECHANICS: if the player is dying as Cube, consider timing (early jump, late jump, misclick). If Ship, consider straight-fly or micro-corrections. If Wave, consider spam control. Use this to give specific mechanical advice.
4. If there is a clear choke point (3+ deaths in the same window), focus your advice there. If runs are improving, acknowledge it and address the new wall.
5. Only recommend practice levels when they are genuinely applicable to an advanced skill the player clearly struggles with. For main official GD levels being played casually, tell them to just keep playing.
6. Be direct, specific, and concise. Don't pad with generic encouragement. Respond like a coach, not a tutorial.`;

      const geminiRequestBody = {
        systemInstruction: { parts: [{ text: systemPrompt }] },
        contents,
        generationConfig: {
          temperature: 0.5,  // Lower = less hallucination, more precise
          maxOutputTokens: maxTokens + 250 // Provide physical padding so it can finish its sentence
        }
      };

      const model = "gemini-3.5-flash";
      const apiUrl = `https://generativelanguage.googleapis.com/v1beta/models/${model}:generateContent?key=${env.GEMINI_API_KEY}`;

      const response = await fetch(apiUrl, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(geminiRequestBody)
      });

      const data = await response.json();

      if (!response.ok) {
        console.error("Gemini API Error:", JSON.stringify(data));
        return new Response("Error communicating with AI.", { status: 502, headers: { "Access-Control-Allow-Origin": "*" } });
      }

      const text = data.candidates?.[0]?.content?.parts?.[0]?.text || "No advice generated.";

      return new Response(text, {
        headers: { "Content-Type": "text/plain", "Access-Control-Allow-Origin": "*" }
      });
    } catch (e) {
      console.error(e);
      return new Response("Internal Server Error", { status: 500, headers: { "Access-Control-Allow-Origin": "*" } });
    }
  }
};
