# Parameter catalog

The authoritative layout is `PluginProcessor::createParameterLayout()`. This file describes public compatibility surfaces and must change in the same commit as the code.

| Stable ID | Display name | Type | Range / step | Default | Units | Automation | Smoothing | Scope | Introduced |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `masterGain` | Master Gain | Float | -60.00 to +6.00 / 0.01 | -12.00 | dB | Yes | Required before M1 audio is connected | Global | 0.1.0-M0 |

M0 does not yet apply `masterGain` because the shell intentionally emits silence. M1 must add smoothing and audio application without changing the ID, range, or default.
