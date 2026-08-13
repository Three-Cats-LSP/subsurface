# Subsurface Neo WebAssembly target

This directory is the dedicated Qt 6 WebAssembly entry point for Milestone 14.
It deliberately starts outside the native Kirigami packaging layer so browser
support can be added without weakening Android or Windows builds.

The current bootstrap provides:

- a responsive Neo dashboard shell for desktop and mobile browsers;
- secure-context, Web Bluetooth, and Web Serial capability detection;
- a browser-controlled local-file picker and recognized-file boundary;
- honest fallbacks when a browser cannot offer direct hardware access; and
- a reproducible Qt 6.8 / Emscripten build artifact.

It does **not** implement decompression mathematics in JavaScript. The planner,
profile, XML/UDDF import, cloud synchronization, and dive-log models must be
connected to the corresponding shared Subsurface C++ pipelines before those
features are presented as available.
