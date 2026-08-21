# Parameter catalog

The authoritative layout is `PluginProcessor::createParameterLayout()`. This file describes public compatibility surfaces and must change in the same commit as the code.

| Stable ID | Display name | Type | Range / step | Default | Units | Automation | Smoothing | Scope | Introduced |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `masterGain` | Master Gain | Float | -60.00 to +6.00 / 0.01 | -12.00 | dB | Yes | 20 ms linear gain ramp | Global | 0.1.0-M0 |
| `oscWaveform` | Oscillator Waveform | Choice | Sine, Triangle | Sine | — | Yes | Discrete | Global | 0.1.0-M1 |
| `oscLevel` | Oscillator Level | Float | -60.00 to 0.00 / 0.01 | -6.00 | dB | Yes | 10 ms | Per voice | 0.1.0-M1 |
| `subLevel` | Sub Level | Float | -60.00 to 0.00 / 0.01 | -18.00 | dB | Yes | 10 ms | Per voice | 0.1.0-M1 |
| `filterCutoff` | Filter Cutoff | Float skew 0.25 | 20 to 20000 / 1 | 12000 | Hz | Yes | 10 ms | Per voice | 0.1.0-M1 |
| `ampAttack` | Amp Attack | Float skew 0.35 | 0.001 to 5.000 / 0.001 | 0.010 | s | Yes | Envelope rate | Per voice | 0.1.0-M1 |
| `ampDecay` | Amp Decay | Float skew 0.35 | 0.001 to 5.000 / 0.001 | 0.150 | s | Yes | Envelope rate | Per voice | 0.1.0-M1 |
| `ampSustain` | Amp Sustain | Float | 0.000 to 1.000 / 0.001 | 0.800 | ratio | Yes | None in M1 | Per voice | 0.1.0-M1 |
| `ampRelease` | Amp Release | Float skew 0.30 | 0.005 to 10.000 / 0.001 | 0.400 | s | Yes | Envelope rate | Per voice | 0.1.0-M1 |

M1 preserves the original `masterGain` compatibility surface and adds the first playable parameter set. M2 keeps those first nine IDs in their original order and appends every new ID below. IDs are append-only.

## M2 oscillator parameters

| Stable ID | Display name | Type | Range / step | Default | Units | Smoothing / semantics |
| --- | --- | --- | --- | --- | --- | --- |
| `oscAPosition` | Oscillator A Position | Float | 0–1 / .001 | 0 | ratio | 10 ms |
| `oscACoarse` | Oscillator A Coarse | Integer | -36–36 | 0 | semitone | 10 ms combined pitch |
| `oscAFine` | Oscillator A Fine | Float | -100–100 / .1 | 0 | cent | 10 ms combined pitch |
| `oscAPhase` | Oscillator A Phase | Float | 0–1 / .001 | 0 | cycle | Read at note trigger |
| `oscARandomPhase` | Oscillator A Random Phase | Float | 0–1 / .001 | 0 | ratio | Read at note trigger |
| `oscAPan` | Oscillator A Pan | Float | -1–1 / .001 | 0 | ratio | 10 ms |
| `oscAUnison` | Oscillator A Unison | Integer | 1–8 | 1 | voices | 10 ms fixed-lane fade |
| `oscADetune` | Oscillator A Detune | Float | 0–100 / .1 | 12 | cent | 10 ms |
| `oscASpread` | Oscillator A Spread | Float | 0–1 / .001 | .5 | ratio | 10 ms |
| `oscABlend` | Oscillator A Blend | Float | 0–1 / .001 | .5 | ratio | 10 ms |
| `oscAPhaseReset` | Oscillator A Phase Reset | Boolean | Off/On | On | — | Read at note trigger |
| `oscBPosition` | Oscillator B Position | Float | 0–1 / .001 | 0 | ratio | 10 ms |
| `oscBCoarse` | Oscillator B Coarse | Integer | -36–36 | 0 | semitone | 10 ms combined pitch |
| `oscBFine` | Oscillator B Fine | Float | -100–100 / .1 | 0 | cent | 10 ms combined pitch |
| `oscBPhase` | Oscillator B Phase | Float | 0–1 / .001 | 0 | cycle | Read at note trigger |
| `oscBRandomPhase` | Oscillator B Random Phase | Float | 0–1 / .001 | 0 | ratio | Read at note trigger |
| `oscBLevel` | Oscillator B Level | Float | -60–0 / .01 | -60 | dB | 10 ms |
| `oscBPan` | Oscillator B Pan | Float | -1–1 / .001 | 0 | ratio | 10 ms |
| `oscBUnison` | Oscillator B Unison | Integer | 1–8 | 1 | voices | 10 ms fixed-lane fade |
| `oscBDetune` | Oscillator B Detune | Float | 0–100 / .1 | 12 | cent | 10 ms |
| `oscBSpread` | Oscillator B Spread | Float | 0–1 / .001 | .5 | ratio | 10 ms |
| `oscBBlend` | Oscillator B Blend | Float | 0–1 / .001 | .5 | ratio | 10 ms |
| `oscBPhaseReset` | Oscillator B Phase Reset | Boolean | Off/On | On | — | Read at note trigger |

`oscWaveform` remains a deprecated compatibility alias that offsets A's built-in frame position; new work should use `oscAPosition`. `oscLevel` is A's level.

## M2 source and filter parameters

| Stable ID | Display name | Type | Range / choices | Default | Units | Smoothing / semantics |
| --- | --- | --- | --- | --- | --- | --- |
| `subWaveform` | Sub Waveform | Choice | Sine, Triangle | Sine | — | Discrete |
| `subOctave` | Sub Octave | Integer | -2–0 | -1 | octave | Discrete pitch |
| `noiseType` | Noise Type | Choice | White, Pink | White | — | Discrete |
| `noiseLevel` | Noise Level | Float | -60–0 / .01 | -60 | dB | 10 ms |
| `filterMode` | Filter Mode | Choice | Low-pass, High-pass, Band-pass | Low-pass | — | Discrete |
| `filterResonance` | Filter Resonance | Float | 0–1 / .001 | .1 | ratio | 10 ms |
| `filterDrive` | Filter Drive | Float | 0–24 / .01 | 0 | dB | 10 ms |
| `filterKeyTracking` | Filter Key Tracking | Float | 0–1 / .001 | 0 | ratio | Evaluated per sample |
| `filterEnvAmount` | Filter Envelope Amount | Float | -8–8 / .01 | 0 | octave | Evaluated per sample |

## M2 filter and auxiliary envelopes

All envelope parameters are automatable and per voice. Attack/decay use skew .35; release uses skew .30.

| Stable ID | Display name | Range / step | Default | Units |
| --- | --- | --- | --- | --- |
| `filterEnvAttack` | Filter Env Attack | .001–5 / .001 | .010 | s |
| `filterEnvDecay` | Filter Env Decay | .001–5 / .001 | .200 | s |
| `filterEnvSustain` | Filter Env Sustain | 0–1 / .001 | 0 | ratio |
| `filterEnvRelease` | Filter Env Release | .005–10 / .001 | .300 | s |
| `auxEnvAttack` | Aux Env Attack | .001–5 / .001 | .050 | s |
| `auxEnvDecay` | Aux Env Decay | .001–5 / .001 | .300 | s |
| `auxEnvSustain` | Aux Env Sustain | 0–1 / .001 | .500 | ratio |
| `auxEnvRelease` | Aux Env Release | .005–10 / .001 | .500 | s |

## M2 LFO parameters

The same exact six-field layout is appended for each of four LFOs. Rate changes preserve phase; shape/sync/retrigger are discrete. All are automatable and per voice except free-running LFO phase, which is engine-global when Retrigger is Off.

| Stable IDs | Field | Range / choices | Default |
| --- | --- | --- | --- |
| `lfo1Shape`, `lfo2Shape`, `lfo3Shape`, `lfo4Shape` | Shape | Sine, Triangle, Saw, Square | Sine |
| `lfo1Rate`, `lfo2Rate`, `lfo3Rate`, `lfo4Rate` | Rate | .01–30 Hz / .01, skew .30 | 1 Hz |
| `lfo1SyncDivision`, `lfo2SyncDivision`, `lfo3SyncDivision`, `lfo4SyncDivision` | Division | 4 bars, 2 bars, 1 bar, 1/2, 1/4, 1/8, 1/16 | 1/4 |
| `lfo1Phase`, `lfo2Phase`, `lfo3Phase`, `lfo4Phase` | Phase | 0–1 / .001 | 0 |
| `lfo1TempoSync`, `lfo2TempoSync`, `lfo3TempoSync`, `lfo4TempoSync` | Tempo Sync | Off/On | Off |
| `lfo1Retrigger`, `lfo2Retrigger`, `lfo3Retrigger`, `lfo4Retrigger` | Retrigger | Off/On | On |

## Modulation route state

Routes are versioned plug-in state rather than host parameters. A route contains registered source and destination enums, a finite bipolar-normalized amount from -1 to 1, Linear/Exponential/S-curve, and Enabled. The complete matrix is capped at 32 and rejected transactionally if any field is invalid.
