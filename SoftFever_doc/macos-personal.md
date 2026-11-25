# OrcaSlicer Personal for Apple Silicon

The DMG contains **OrcaSlicer Personal.app**. Drag it to Applications and open it from Finder. This build is ad-hoc signed for personal use. On first launch, choose a printer preset and filament in the setup wizard. You can import, slice, preview and export a model with the network plug-in disabled. Settings normally live in `~/Library/Application Support/OrcaSlicer-Personal`. An explicit `--datadir` takes precedence.

## Sending a sliced plate

| Workflow | How to use it |
| --- | --- |
| Offline | Slice, inspect Preview, then use **File → Export → Export plate sliced file**. Save the resulting `.gcode.3mf` locally or on removable media. |
| Bambu Connect | Install Bambu Connect for Apple Silicon. Slice the plate, then choose **File → Export → Open sliced plate in Bambu Connect**. Choose a filename. Connect handles printer selection, sign-in, AMS choices and print confirmation. If macOS cannot open Connect, Orca keeps the saved file. [Bambu Connect](https://wiki.bambulab.com/en/software/bambu-connect) · [Integration interface](https://wiki.bambulab.com/en/software/third-party-integration) |
| Direct LAN | Enable the optional network plug-in in Orca’s setup or Preferences. On a compatible printer, select LAN Only and the vendor’s Developer Mode where required by its firmware. Pair from **Device** using the printer’s displayed IP and access code. Choose the matching plate, filament and AMS slot before sending. Camera and storage availability depend on the printer, firmware and inserted media. |

If the connection drops after sending, reconnect and inspect the current job on the printer or in the Device tab before submitting again. The personal build keeps printer-reported errors visible.

## Native plug-in setup and repair

The app requests the Mac plug-in under OrcaSlicer’s client identity. The bundled `orca-native-plugin-check` loads the candidate networking and camera libraries in a separate process and checks the expected network version and required symbols. A verified update is activated by exchanging directories on the same volume. Printer profiles are stored outside those directories.

- **Missing plug-in.** Use **Enable network plug-in** in Preferences and follow the download prompt. Offline slicing works while the plug-in is missing.
- **Interrupted or incompatible download.** Retry the download. An incomplete candidate or failed library check leaves the active directory intact. The app log records the installer error. To diagnose a particular extracted directory, run the verifier below.
- **Previously working installation.** Quit Orca before changing plug-in files. In the personal settings folder, rename `plugins` to an unused name and rename `plugins-previous` to `plugins` if available. Relaunch. To switch back, quit again and reverse the renames. Keep the rest of the settings folder in place.
- **Account or printer unavailable.** Bambu Connect and the Device tab need an appropriate account or paired LAN printer for live control. You can continue to export sliced plates locally.

```bash
"/Applications/OrcaSlicer Personal.app/Contents/MacOS/orca-native-plugin-check" \
  "$HOME/Library/Application Support/OrcaSlicer-Personal/plugins"
```

## Build and verify

Use Apple Silicon, Xcode with its command line tools, CMake, Ninja, automake, libtool, texinfo and gettext. From the checkout run the following commands. Dependency compilation downloads the versions pinned by the repository. The personal build uses a space-free dependency cache under `~/Library/Caches/OrcaSlicer-build`. Set `ORCA_MAC_DEPS_BUILD_ROOT` to reuse another cache on the same Mac.

```bash
./build_release_macos.sh -Pdx -a arm64 -t 13.0
./build_release_macos.sh -Psx -a arm64 -t 13.0
app=build/arm64/OrcaSlicer/OrcaSlicer.app
tests/macos/offline_slice.sh "$app" build/verification
scripts/package_macos_personal.sh "$app" build/OrcaSlicer-Personal-arm64.dmg

cmake -S . -B build/arm64 -DBUILD_TESTS=ON
cmake --build build/arm64 --config Release --target native_printer_workflow
ctest --test-dir build/arm64 -C Release -R '^native_printer_workflow$' --output-on-failure
```

Use a fresh output directory for each slice and a new name for each DMG. The manual GitHub Actions workflow **Personal macOS arm64 build** builds the same targets and retains its DMG as a run artifact.

The printer fixture loads only in the test runner. It has a fake account, an in-memory printer, and local storage. It exercises the app’s plug-in adapters and file browser. The camera checks use the actual vendor player and generated loopback video. With FFmpeg, OpenSSL and MediaMTX 1.21.4 on PATH, run the native tests against your extracted Mac plug-in.

```bash
tests/macos/native_runtime.sh "/path/to/native/plugins" "/path/to/network_plugin.zip"
```

## When the printer arrives

Record the printer model, firmware, nozzle and AMS configuration. Use a small sliced model and record the result of each applicable check.

- Pair over LAN and confirm the Device tab identifies the correct machine. In Connect, sign in and confirm the printer bound to that account.
- Send one plate by each supported route. Check physical start, progress, pause, resume and cancellation. Disconnect and reconnect during an active job and confirm the job ID and print count.
- Test the external spool and an explicitly selected AMS slot. Compare the selected mapping with the filament actually fed.
- Open the camera, stop and reopen it, then sleep and wake the Mac and check that the live feed resumes.
- On printer storage, upload a disposable file, list it, download it and compare its checksum. Delete only that test file and refresh the list.
