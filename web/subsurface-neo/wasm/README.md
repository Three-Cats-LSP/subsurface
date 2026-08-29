# Subsurface Neo WebAssembly target

This directory is the dedicated Qt 6 WebAssembly entry point for Milestone 14.
It deliberately starts outside the native Kirigami packaging layer so browser
support can be added without weakening Android or Windows builds.

The current bootstrap provides:

- a responsive Neo dashboard shell for desktop and mobile browsers;
- secure-context, Web Bluetooth, and Web Serial capability detection;
- a mock-tested browser-device transport state machine that isolates selection,
  connection, cancellation, errors, and disconnect behavior from libdivecomputer;
- a browser-controlled local-file picker;
- a shared C++ core reader for native Subsurface XML logs;
- real dashboard totals and recent-dive summaries from the selected log;
- a complete dive list with native-summary search plus year and dive-mode filters;
- selectable dive-detail pages with recorded profile samples;
- durable IndexedDB-backed browser editing for location, buddy, notes, mode, gas, and gear, with explicit restore and forget controls;
- selected-dive JSON, dive-list CSV, and native XML session-backup downloads;
- a responsive draft-planner workspace with SAC/cylinder/reserve gas estimates, configurable safety stops, and explicit native-planner safety warnings;
- a revision/checksum cloud-manifest conflict preview that never overwrites silently;
- an interactive depth, temperature, NDL, and cylinder-pressure chart;
- recorded TTS, deco-stop, CNS, and CCR-setpoint values in the sample inspector;
- honest fallbacks when a browser cannot offer direct hardware access; and
- a reproducible Qt 6.8 / Emscripten build artifact.

It does **not** implement decompression mathematics in JavaScript. Foreign-format
imports, authenticated cloud transfer, calculated planner decompression, and writing
back to the originally selected local file must be connected to the corresponding shared Subsurface C++ pipelines
before those features are presented as available. The current browser reader
intentionally accepts only the native XML format emitted by `core/save-xml.cpp`.
The transport controller is intentionally backed by a capability-only adapter in
production until the JavaScript byte-stream bridge and hardware validation land;
it never presents a simulated connection as a real dive computer.
