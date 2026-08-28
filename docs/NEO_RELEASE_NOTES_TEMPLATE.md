# Subsurface Neo {{VERSION}}

Unpublished release-candidate bundle built from `{{SOURCE_SHA}}`.

## Highlights

- Modern responsive dashboard, dive list, dive details, editor, planner, sites, statistics, equipment, import, settings, and portability workflows.
- Native Subsurface profile, decompression, planner, import/export, and libdivecomputer foundations retained.
- Google Drive, Dropbox, and Subsurface Cloud compatibility workflows with secure native credential storage.
- Android arm64 development APK, Windows x64 portable archive, update-manifest preview, and SHA-256 checksums.

## Candidate limitations

- This bundle is for engineering validation and is not a public signed release.
- Production OAuth publication, final legal approval, release signing, physical-device hardware coverage, and the full offline/multi-device matrix remain release gates.
- Browser device selection and byte-stream transport are still under development.

## Validation checklist

- [ ] Android build and installation smoke test
- [ ] Windows launch and portable-package smoke test
- [ ] Dive-log open/save and backup/restore round trip
- [ ] Planner regression suite
- [ ] Cloud conflict and offline scenarios
- [ ] Bluetooth/USB/serial hardware matrix
- [ ] Accessibility and keyboard review
- [ ] Checksums verified
