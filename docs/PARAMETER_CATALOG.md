# Parameter catalog

The authoritative layout is `PluginProcessor::createParameterLayout()`. This file describes public compatibility surfaces and must change in the same commit as the code.

| Stable ID | Display name | Type | Range / step | Default | Units | Automation | Smoothing | Scope | Introduced |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `masterGain` | Master Gain | Float | -60.00 to +6.00 / 0.01 | -12.00 | dB | Yes | 20 ms linear gain ramp | Global | 0.1.0-M0 |
| `oscWaveform` | Oscillator Waveform | Choice | Sine, Triangle | Sine | — | Yes | Discrete | Global | 0.1.0-M1 |
| `oscLevel` | Oscillator Level | Float | -60.00 to 0.00 / 0.01 | -6.00 | dB | Yes | None in M1 | Global | 0.1.0-M1 |
| `subLevel` | Sub Level | Float | -60.00 to 0.00 / 0.01 | -18.00 | dB | Yes | None in M1 | Global | 0.1.0-M1 |
| `filterCutoff` | Filter Cutoff | Float skew 0.25 | 20 to 20000 / 1 | 12000 | Hz | Yes | None in M1 | Global | 0.1.0-M1 |
| `ampAttack` | Amp Attack | Float skew 0.35 | 0.001 to 5.000 / 0.001 | 0.010 | s | Yes | Envelope rate | Per voice | 0.1.0-M1 |
| `ampDecay` | Amp Decay | Float skew 0.35 | 0.001 to 5.000 / 0.001 | 0.150 | s | Yes | Envelope rate | Per voice | 0.1.0-M1 |
| `ampSustain` | Amp Sustain | Float | 0.000 to 1.000 / 0.001 | 0.800 | ratio | Yes | None in M1 | Per voice | 0.1.0-M1 |
| `ampRelease` | Amp Release | Float skew 0.30 | 0.005 to 10.000 / 0.001 | 0.400 | s | Yes | Envelope rate | Per voice | 0.1.0-M1 |

M1 preserves the original `masterGain` compatibility surface and adds the first playable parameter set. IDs are append-only. Continuous oscillator/filter automation is not yet smoothed and must be addressed before release hardening.
