# Subsurface Neo WebAssembly target

This directory is the dedicated Qt 6 WebAssembly entry point for Milestone 14.
It deliberately starts outside the native Kirigami packaging layer so browser
support can be added without weakening Android or Windows builds.

The current bootstrap provides:

- a responsive Neo dashboard shell for desktop and mobile browsers;
- secure-context, Web Bluetooth, and Web Serial capability detection;
- a browser-controlled local-file picker;
- a shared C++ core reader for native Subsurface XML logs;
- real dashboard totals and recent-dive summaries from the selected log;
- honest fallbacks when a browser cannot offer direct hardware access; and
- a reproducible Qt 6.8 / Emscripten build artifact.

It does **not** implement decompression mathematics in JavaScript. Foreign-format
imports, full profile processing, editing, cloud synchronization, and the
planner must be connected to the corresponding shared Subsurface C++ pipelines
before those features are presented as available. The current browser reader
intentionally accepts only the native XML format emitted by `core/save-xml.cpp`.
