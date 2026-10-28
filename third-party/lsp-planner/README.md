# LSP D-Planner+ calculation sources

Vendored from Three-Cats-LSP/LSP_D-planner-plus, commit
`05fef18bade6354cebcc60df01f83e0ac4cc3b27`, version **3.2.0**.
`SOURCE.json` records the exact upstream file checksums and earlier research
documents. The upstream README declares the project MIT licensed; that revision
does not contain a standalone LICENSE file. Retain this attribution and the
original source comments when distributing these files.

The four top-level JavaScript files are byte-for-byte upstream source. They
contain the actual ZHL, CCR/pSCR and VPM calculations, shared exposure integration
and existing input guards. The schedule runner's DOM functions are not called
by the native host. No proprietary application binaries or reconstructed
proprietary implementation code are included.

Qt's JavaScript parser cannot load all of the original syntax. `qt-runtime/`
contains a reproducible syntax translation using the locked Babel toolchain:

```
cd tools/lsp-planner
npm ci
node build-runtime.cjs --check
```

Omit `--check` only to regenerate from the checksum-verified source. Never edit
the generated files to change calculations. End users need neither Node nor a
browser; these files run in an isolated QJSEngine in the Windows/Android app.

The adoption is not complete until the shared planner UI, plan persistence,
exports and repetitive loading all carry the LSP engine identity and effective
settings. Existing Subsurface plans and their original calculation identity
must remain readable. Native-mapping preset labels are not LSP engine parity.
