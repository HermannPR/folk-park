# FL Studio routing note

Target host: FL Studio 26.1.4.5356 on Intel macOS 15.7.9.

Image-Line documents that FL Studio for macOS supports 64-bit VST3 and loads it through the Plugin Wrapper. Folk Park therefore ships an x86_64 VST3 instrument with no audio input, stereo audio output, MIDI input, and declared MIDI output. Sources:

- https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins_supported.htm
- https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/wrapper.htm

## Local installation and discovery

1. Build Release with `cmake --preset macos-x86_64-release` and `cmake --build --preset macos-x86_64-release`.
2. Install only the built bundle with `./scripts/install_user_vst3.sh release` into the current user's `Library/Audio/Plug-Ins/VST3/folk park.vst3`.
3. In FL Studio, open the Plugin Manager, start a scan/rescan, and confirm `folk park` is classified as a generator/instrument.
4. Add it to the Channel Rack and record discovery/editor/MIDI/audio/state evidence in `docs/FL_STUDIO_TEST_MATRIX.md`.

## Direct MIDI output proof

FL Studio's Wrapper MIDI settings expose input and output port numbers. Set Folk Park's Wrapper output port to an unused value, then set the receiving instrument's Wrapper input port to the same value. Generate/route a bounded clip and confirm note timing, note-offs, transport stop, and panic behavior. This host-dependent path is secondary in 0.1; drag-and-drop MIDI is primary.

Do not change wrapper compatibility options merely to obtain a pass. Record defaults first, then record any workaround and its consequence. Keep `Send All Notes Off when playback stops` enabled for the normal test unless a documented failure requires otherwise.
