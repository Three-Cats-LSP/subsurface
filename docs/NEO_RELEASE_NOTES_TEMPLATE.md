# Subsurface Neo {{VERSION}}

Unpublished release-candidate bundle built from `{{SOURCE_SHA}}`.

## Highlights

- Modern responsive dashboard, dive list, dive details, editor, planner, sites, statistics, equipment, import, settings, and portability workflows.
- Native Subsurface profile, import/export, libdivecomputer and existing planner compatibility retained. The new shared planner uses the pinned LSP D-Planner+ engine; its exact candidate validation is recorded in the attached evidence.
- Google Drive is the selectable Neo sync provider, with secure native credential storage. Existing Dropbox implementation and credentials remain preserved for a future version.
- Windows x64 portable archive, unpublished update-manifest preview and SHA-256 checksums. An included Android development APK is compile evidence; physical Android acceptance is deferred.

## Candidate limitations

- This bundle is for engineering validation and is not a public signed release.
- Production OAuth publication, final legal approval, release signing, physical-device hardware coverage, and the full offline/multi-device matrix remain release gates.
- Windows and Android are the product targets. Linux desktop and browser products are outside this release scope.
- Coordinated workspace sync remains development-gated until sustainable history, separate Neo metadata synchronization and the remaining Windows reliability gates pass. Schema 1/2 objects are protected and cleanup remains disabled.
- Planner identity, pinned source and effective settings must match the candidate’s passed UI/save/export evidence. Unproven external-compatibility claims remain disabled.

## Validation checklist

- [ ] Android build (physical installation and runtime acceptance deferred)
- [ ] Windows launch and portable-package smoke test
- [ ] Dive-log open/save and backup/restore round trip
- [ ] Planner regression suite
- [ ] Cloud conflict and offline scenarios
- [ ] Bluetooth/USB/serial hardware matrix
- [ ] Accessibility and keyboard review
- [ ] Checksums verified
