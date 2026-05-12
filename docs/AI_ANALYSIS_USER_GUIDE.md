# ROCm Optiq — AI Analysis Feature User Guide

This guide walks you through using the **AI Analysis** feature in ROCm Optiq, which connects to an LLM (Large Language Model) via AMD's LLM Gateway to automatically analyze your ROCm profiling traces.

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Setting Up Your API Key](#2-setting-up-your-api-key)
3. [Opening a Trace File](#3-opening-a-trace-file)
4. [Running AI Analysis](#4-running-ai-analysis)
5. [Understanding the Results](#5-understanding-the-results)
6. [Menu Reference](#6-menu-reference)
7. [Troubleshooting](#7-troubleshooting)

---

## 1. Prerequisites

Before using AI Analysis, ensure you have:

- **ROCm Optiq** built and running (see `BUILDING.md` for build instructions)
- **An LLM Gateway API Key** from your organization's LLM Gateway portal
- **Your User ID** (typically your Windows/network username)
- **A trace file** (`.db`, `.rpd`, or `.rpv`) to analyze
- **Network access** to `llm-api.amd.com`

---

## 2. Setting Up Your API Key

You only need to do this once — the key is saved across sessions.

### Steps:

1. **Launch ROCm Optiq** by running `roc-optiq.exe`
2. In the menu bar, click **AI Analysis** → **API Key**
3. A dialog appears with two fields:

   **[Screenshot Description: API Key Dialog]**
   > A modal dialog titled "API Key" with two input fields:
   > - **User ID** — Pre-populated with your Windows username (e.g., `njobypet`). Edit if needed.
   > - **API Key** — A password-masked field. Paste your LLM Gateway API key here.
   > - Two buttons at the bottom: **Save** and **Cancel**.

4. Enter your **User ID** (usually pre-filled with your Windows login)
5. Paste your **API Key** (the key from your LLM Gateway portal, starts with your gateway key prefix)
6. Click **Save**

Your credentials are now stored in `%LOCALAPPDATA%\AMD\ROCm-Optiq\settings_application.json` and will persist across application restarts.

---

## 3. Opening a Trace File

Before running analysis, you need to load a profiling trace:

1. Click **File** → **Open** (or drag-and-drop a file onto the window)
2. Select a trace file:
   - **`.db`** — SQLite database trace from ROCm Systems Profiler
   - **`.rpd`** — ROCm Profile Data trace
   - **`.rpv`** — Optiq project file (includes trace + saved customizations)

   **[Screenshot Description: File Open Dialog]**
   > The native Windows file dialog showing supported file types: "All Supported (*.db, *.rpd, *.yaml, *.rpv, *.csv)".

3. The trace loads and the **Timeline View** appears with tracks showing GPU events, API calls, and counter data.

   **[Screenshot Description: Loaded Trace]**
   > The main Optiq window showing:
   > - Left panel: System Topology Tree with nodes/GPUs/queues
   > - Center: Timeline tracks with colored event bars
   > - Bottom: Advanced Details area with Event Table
   > - Top: Toolbar with flow, annotation, search, and bookmark controls

---

## 4. Running AI Analysis

With a trace loaded and API key configured:

1. Click **AI Analysis** → **Start Analysis**

   **[Screenshot Description: AI Analysis Menu]**
   > The menu bar showing "AI Analysis" expanded with two items:
   > - **API Key** — Opens the credential configuration dialog
   > - **Start Analysis** — Starts the LLM analysis (enabled only when a trace is loaded and API key is set)
   > - A grayed-out "Analysis in progress..." label appears while running

2. A **status popup** appears showing live progress:

   **[Screenshot Description: Analysis Status Popup]**
   > A small modal dialog titled "AI Analysis Status" showing:
   > - An animated red spinner (rotating dots)
   > - Status text: "Sending data to LLM gateway..."
   > - A **Close** button to dismiss the popup (analysis continues in background)

   The status progresses through:
   - `"Extracting profiling data from trace..."` — Reading GPU utilization, kernel metrics, timeline data
   - `"Sending data to LLM gateway..."` — HTTP request to the LLM API in progress
   - `"Analysis complete. Opening results..."` — Response received, generating report

3. When complete, your **default web browser** opens automatically with the HTML report.

---

## 5. Understanding the Results

The AI Analysis report opens as a styled HTML page in your browser.

**[Screenshot Description: HTML Analysis Report]**
> A dark-themed HTML page in the browser with the header "ROCm Optiq — AI Analysis" and sections including:
> - **GPU Utilization** — Table showing Peak GPU Utilization, Average GPU Utilization, GPU Idle Time, Kernel Execution Count, and Top 3 Kernels by Total Execution Time
> - **Kernel-Level Insights** — Analysis of individual kernel performance
> - **Memory Usage** — GPU memory utilization patterns and memory transaction analysis
> - **CPU Usage** — Host-side activity assessment
> - **Performance Recommendations** — Actionable suggestions for optimization

### Report Sections

| Section | Description |
|---------|-------------|
| **GPU Utilization** | Overall GPU busy percentage, idle time analysis |
| **Top Kernels** | Ranked list of kernels by execution time with invocation counts |
| **Memory Usage** | GPU memory utilization percentages and memory transfer patterns |
| **CPU Usage** | Host-side CPU activity estimation |
| **Recommendations** | Specific suggestions to improve performance |

### Report Location

The HTML file is saved to:
```
%LOCALAPPDATA%\AMD\ROCm-Optiq\ai_analysis_results.html
```
You can reopen it anytime from this path.

---

## 6. Menu Reference

### AI Analysis Menu

| Menu Item | Description | Availability |
|-----------|-------------|-------------|
| **API Key** | Opens dialog to configure User ID and API Key | Always available |
| **Start Analysis** | Sends trace data to LLM and generates HTML report | Requires: API key set + trace loaded + no analysis running |

### Data Sent to the LLM

When you run analysis, Optiq extracts and sends:

- **Trace file path** (for context)
- **GPU utilization metrics** — GFX utilization %, memory utilization %
- **Top kernel metrics** — Name, invocations, total/min/max execution time, percentage
- **Timeline overview** — Start time, end time, duration, track count
- **Aggregated metrics** per device/node if available

> **Privacy Note:** Only summary metrics are sent, not the full trace database. The API key and user ID are sent as HTTP headers for authentication.

---

## 7. Troubleshooting

### "Start Analysis" is grayed out

- **No API key set** — Go to AI Analysis → API Key and enter your credentials
- **No trace loaded** — Open a trace file first via File → Open
- **Analysis already running** — Wait for the current analysis to complete

### "Error: Failed to connect to llm-api.amd.com"

- Check your network connection
- Ensure you can reach `https://llm-api.amd.com` from your machine
- VPN may be required if you're off the corporate network

### "Error: API returned status 401"

- Your API key is invalid or expired
- Regenerate your key from the LLM Gateway portal
- Ensure the User ID field is correct

### "Error: API returned status 404"

- The gateway endpoint may have changed
- Contact your LLM Gateway administrator

### "Error: API returned status 429"

- Rate limit exceeded — wait a moment and try again

### Report doesn't open in browser

- Check if the file was saved: `%LOCALAPPDATA%\AMD\ROCm-Optiq\ai_analysis_results.html`
- Open it manually in your browser
- Ensure a default browser is configured on your system

### Status popup says "Sending data..." for a long time

- LLM responses can take 30-60 seconds for large traces
- You can close the status popup — analysis continues in the background
- The browser will open when results arrive

---

## Architecture Overview

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│   ROCm Optiq    │     │  AMD LLM Gateway  │     │   LLM (GPT)     │
│                 │     │                   │     │                 │
│ 1. Extract      │────▶│ 2. Authenticate   │────▶│ 3. Analyze      │
│    trace data   │     │    via Ocp-Apim   │     │    profiling    │
│                 │◀────│    -Subscription  │◀────│    data         │
│ 5. Render HTML  │     │    -Key header    │     │                 │
│    & open       │     │                   │     │ 4. Return       │
│    browser      │     │                   │     │    analysis     │
└─────────────────┘     └──────────────────┘     └─────────────────┘
```

### HTTP Request Details

- **Endpoint:** `POST https://llm-api.amd.com/OnPrem/chat/completions`
- **Headers:**
  - `Content-Type: application/json`
  - `Ocp-Apim-Subscription-Key: <your-api-key>`
  - `user: <your-user-id>`
- **Model:** `GPT-oss-20B`
- **Format:** OpenAI-compatible chat completions API

---

## Files Modified for AI Analysis Feature

| File | Changes |
|------|---------|
| `src/view/src/rocprofvis_appwindow.h` | Added AI Analysis menu, API key dialog, status popup, LLM integration methods and members |
| `src/view/src/rocprofvis_appwindow.cpp` | Full implementation: menu rendering, API key save/load, HTTP calls via WinHTTP, HTML report generation, status popup with spinner |
| `src/view/src/rocprofvis_settings_manager.h` | Added `ai_api_key` and `ai_user_id` to `InternalSettings` |
| `src/view/src/rocprofvis_settings_manager.cpp` | Serialize/deserialize API key and user ID to settings JSON |
| `src/view/src/rocprofvis_trace_view.h` | Added `GetDataProvider()` public accessor |
| `CMakeLists.txt` | Added `winhttp` library link for Windows HTTP support |
