# Subsurface Neo release checklist

Use **Subsurface Neo Release Candidate** first. It builds the exact `modern-ui`
head and uploads an unpublished APK/Windows ZIP bundle with release notes,
`update.json`, and `SHA256SUMS.txt`. It does not create a tag, GitHub Release,
or website update notification.

Before the publishing workflow is authorized:

1. Verify every candidate checksum.
2. Install and smoke-test Android and Windows artifacts.
3. Run planner, cloud, WebAssembly, import/export, and backup/restore gates.
4. Complete physical dive-computer coverage appropriate for the release.
5. Confirm OAuth publication, privacy/terms approval, and signing credentials.
6. Review and finalize release notes.
7. Run **Subsurface Neo Release** only after explicit publication approval.

The publishing workflow creates an immutable `neo-vVERSION` tag, waits for the
tagged native builds, uploads the APK and Windows portable ZIP with checksums,
and promotes the update manifest only after the platform builds succeed.
