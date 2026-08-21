#pragma once

#include <array>

namespace folkpark::parameterIds
{
inline constexpr auto masterGain = "masterGain";
inline constexpr auto oscillatorWaveform = "oscWaveform";
inline constexpr auto oscillatorLevel = "oscLevel";
inline constexpr auto oscillatorAPosition = "oscAPosition";
inline constexpr auto oscillatorACoarse = "oscACoarse";
inline constexpr auto oscillatorAFine = "oscAFine";
inline constexpr auto oscillatorAPhase = "oscAPhase";
inline constexpr auto oscillatorARandomPhase = "oscARandomPhase";
inline constexpr auto oscillatorAPan = "oscAPan";
inline constexpr auto oscillatorAUnison = "oscAUnison";
inline constexpr auto oscillatorADetune = "oscADetune";
inline constexpr auto oscillatorASpread = "oscASpread";
inline constexpr auto oscillatorABlend = "oscABlend";
inline constexpr auto oscillatorAPhaseReset = "oscAPhaseReset";
inline constexpr auto oscillatorBPosition = "oscBPosition";
inline constexpr auto oscillatorBCoarse = "oscBCoarse";
inline constexpr auto oscillatorBFine = "oscBFine";
inline constexpr auto oscillatorBPhase = "oscBPhase";
inline constexpr auto oscillatorBRandomPhase = "oscBRandomPhase";
inline constexpr auto oscillatorBLevel = "oscBLevel";
inline constexpr auto oscillatorBPan = "oscBPan";
inline constexpr auto oscillatorBUnison = "oscBUnison";
inline constexpr auto oscillatorBDetune = "oscBDetune";
inline constexpr auto oscillatorBSpread = "oscBSpread";
inline constexpr auto oscillatorBBlend = "oscBBlend";
inline constexpr auto oscillatorBPhaseReset = "oscBPhaseReset";
inline constexpr auto subWaveform = "subWaveform";
inline constexpr auto subOctave = "subOctave";
inline constexpr auto subLevel = "subLevel";
inline constexpr auto noiseType = "noiseType";
inline constexpr auto noiseLevel = "noiseLevel";
inline constexpr auto filterMode = "filterMode";
inline constexpr auto filterCutoff = "filterCutoff";
inline constexpr auto filterResonance = "filterResonance";
inline constexpr auto filterDrive = "filterDrive";
inline constexpr auto filterKeyTracking = "filterKeyTracking";
inline constexpr auto filterEnvelopeAmount = "filterEnvAmount";
inline constexpr auto ampAttack = "ampAttack";
inline constexpr auto ampDecay = "ampDecay";
inline constexpr auto ampSustain = "ampSustain";
inline constexpr auto ampRelease = "ampRelease";
inline constexpr auto filterEnvelopeAttack = "filterEnvAttack";
inline constexpr auto filterEnvelopeDecay = "filterEnvDecay";
inline constexpr auto filterEnvelopeSustain = "filterEnvSustain";
inline constexpr auto filterEnvelopeRelease = "filterEnvRelease";
inline constexpr auto auxiliaryEnvelopeAttack = "auxEnvAttack";
inline constexpr auto auxiliaryEnvelopeDecay = "auxEnvDecay";
inline constexpr auto auxiliaryEnvelopeSustain = "auxEnvSustain";
inline constexpr auto auxiliaryEnvelopeRelease = "auxEnvRelease";

inline constexpr std::array lfoShape{"lfo1Shape", "lfo2Shape", "lfo3Shape", "lfo4Shape"};
inline constexpr std::array lfoRate{"lfo1Rate", "lfo2Rate", "lfo3Rate", "lfo4Rate"};
inline constexpr std::array lfoSyncDivision{
    "lfo1SyncDivision", "lfo2SyncDivision", "lfo3SyncDivision", "lfo4SyncDivision"};
inline constexpr std::array lfoPhase{"lfo1Phase", "lfo2Phase", "lfo3Phase", "lfo4Phase"};
inline constexpr std::array lfoTempoSync{"lfo1TempoSync", "lfo2TempoSync", "lfo3TempoSync", "lfo4TempoSync"};
inline constexpr std::array lfoRetrigger{"lfo1Retrigger", "lfo2Retrigger", "lfo3Retrigger", "lfo4Retrigger"};

inline constexpr auto distortionBypass = "distBypass";
inline constexpr auto distortionDrive = "distDrive";
inline constexpr auto distortionMix = "distMix";
inline constexpr auto distortionOutput = "distOutput";
inline constexpr auto chorusBypass = "chorusBypass";
inline constexpr auto chorusRate = "chorusRate";
inline constexpr auto chorusDepth = "chorusDepth";
inline constexpr auto chorusMix = "chorusMix";
inline constexpr auto delayBypass = "delayBypass";
inline constexpr auto delayDivision = "delayDivision";
inline constexpr auto delayFeedback = "delayFeedback";
inline constexpr auto delayMix = "delayMix";
inline constexpr auto reverbBypass = "reverbBypass";
inline constexpr auto reverbRoomSize = "reverbRoomSize";
inline constexpr auto reverbDamping = "reverbDamping";
inline constexpr auto reverbMix = "reverbMix";
inline constexpr auto compressorBypass = "compBypass";
inline constexpr auto compressorThreshold = "compThreshold";
inline constexpr auto compressorRatio = "compRatio";
inline constexpr auto compressorAttack = "compAttack";
inline constexpr auto compressorRelease = "compRelease";
inline constexpr auto compressorMakeup = "compMakeup";
inline constexpr auto compressorMix = "compMix";
inline constexpr auto eqBypass = "eqBypass";
inline constexpr auto eqLowGain = "eqLowGain";
inline constexpr auto eqMidFrequency = "eqMidFrequency";
inline constexpr auto eqMidGain = "eqMidGain";
inline constexpr auto eqMidQ = "eqMidQ";
inline constexpr auto eqHighGain = "eqHighGain";
}
