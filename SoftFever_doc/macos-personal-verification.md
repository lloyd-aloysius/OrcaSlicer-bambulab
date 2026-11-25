# Apple Silicon personal build verification

On 25 November 2026, the packaged app launched from Finder, sliced a model with all network access denied, and passed its native plug-in check. The GUI also exported an A1 plate to the installed Bambu Connect app. Physical printer checks remain open in ORCA-3.

| Build record | Value |
| --- | --- |
| Source | `macos/native-personal` at `f5e590cc89eda63f7613fa5333f847c811e16a7e` |
| Fork base | `1b556d73f80e69f512881eab3331e6e9f70e5bf5` |
| Artifact | `build/personal/OrcaSlicer-Personal-arm64.dmg`, 211,923,135 bytes |
| DMG SHA-256 | `d921c7db42212df7700782b0372b5803a0c4e1629825a0d35cd7a3a2d6e82553` |
| Host | Apple Silicon, macOS 27.0.1 (26A434), Xcode 27.2 (27B52), CMake 4.4.5, Ninja 1.11.1.git.kitware.jobserver-1 |
| Bundle | `com.lloydaloysius.OrcaSlicerPersonal`, ad-hoc signed locally |
| Vendor components tested separately | Bambu Mac plug-in 02.05.02.58 and Bambu Connect 2.6.0-beta.0 |

## Checks against the packaged build

- The Release build completed all 663 steps. The DMG passed `hdiutil verify`. The app passed strict, recursive code-signature verification on the read-only mounted volume and again after copying it off the image.
- Both the app and its plug-in checker were arm64 executables with a macOS 13.0 deployment target. Their linked libraries resolved to macOS system paths. The run used the host OS above. Compatibility on macOS 13 itself and distribution through Gatekeeper with Developer ID notarization remain unverified.
- Finder opened the app copied from the DMG. An isolated portable profile with networking disabled reached the Untitled editor, quit and reopened from Finder in a new process.
- The executable on the mounted DMG sliced the repository’s 20 mm cube under a macOS sandbox that denied all network access. It produced 313,139 bytes of G-code with extrusion moves. The standalone G-code matched the bytes in the sliced 3MF and the checksum stored inside it.
- The packaged plug-in checker loaded the networking and camera libraries installed by the setup wizard. It started and destroyed a signed-out agent under the same network restriction.
- The targeted CTest passed. The filament-printing suite also passed all 196 assertions across 14 test cases.

## Device integration exercised on the Mac

| Component | Observation |
| --- | --- |
| Setup and update | The GUI setup wizard installed the native vendor plug-in, and both libraries were loaded by the GUI process. The installer tests covered a first installation, cancellation, missing files, an incompatible library, a successful update and a repair that retained the last good backup. The tested vendor archive had SHA-256 `59c27a3233c174449aab94503b520ed2d1ac96847785a058ada6b232e5003d81`. |
| Export to Bambu Connect | The GUI sliced the cube with the Bambu Lab A1 preset. Preview showed 100 layers. The File menu exported a plate containing the A1 model identifier, a preview image and 438,994 bytes of G-code with a matching checksum. The installed Bambu Connect app opened that file and displayed its preview. The handoff ended at the import screen. |
| Camera | The native vendor player rendered generated 320 × 240 frames on an AVFoundation display layer from loopback H.264/RTSP and TLS-framed MJPEG streams. The player reported a deliberate connection loss and rendered video again after reopening. |
| Simulated printer | The test loaded the application’s printer, cloud and storage adapters against a local fixture. It exercised discovery, account callbacks, LAN and cloud submissions using the exported bytes, upload cancellation, pause, resume, stop, and reconnect without resubmission. It checked AMS-slot and external-spool choices. |
| Simulated storage | Upload, list, download, checksum comparison and delete passed with a Unicode filename. A rejected deletion kept the file visible. |

Local evidence is in `build/personal/`. The package and offline results are in `release-verify.log`. The installer and camera results are in `runtime-release.log`. The GUI slice and Connect import are captured in `gui-cube-preview.png` and `connect-window.png`.

## Hardware acceptance

Real account binding, pairing, starting a physical job, printer-reported progress and controls, filament feed, onboard storage, the printer’s live camera, and Mac sleep/wake with a connected printer require a device. Use the model and firmware that will actually be paired when running the [hardware checklist](macos-personal.md#when-the-printer-arrives).
