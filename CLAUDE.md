# iosFreeDOS Project Instructions

## Build

After modifying `project.yml` or adding/removing source files, regenerate the Xcode project and open it:
```
xcodegen && open iosFreeDOS.xcodeproj
```
Always run this before telling the user to rebuild.

## Xcode Project Regeneration

When any of these change, run `xcodegen` automatically:
- `project.yml`
- Files added to or removed from `iosFreeDOS/` or `src/`
- Build settings, deployment target, or entitlements

Do not leave the user to run `xcodegen` manually. Run it as part of the workflow.
