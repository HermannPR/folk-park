import assert from "node:assert/strict";
import test from "node:test";
import { chooseAnimationPolicy, parseUiSnapshot, wavetableSamplesPerFrame } from "./protocol.ts";

const wavetable = {
  frameCount: 1,
  samplesPerFrame: wavetableSamplesPerFrame,
  samples: Array.from({ length: wavetableSamplesPerFrame }, (_, index) => Math.sin(index / 8)),
};

const valid = {
  schemaVersion: 1,
  product: "folk park",
  version: "0.1.0",
  architecture: "x86_64",
  activeVoices: 2,
  importStatus: "idle",
  importMessage: "ready",
  modulationRouteCount: 0,
  modulationRoutes: [],
  composition: {
    status: "No composition",
    hasCandidate: false,
    hasAccepted: false,
    candidateMatchesAccepted: false,
    candidateNotes: 0,
    acceptedNotes: 0,
    notes: [],
  },
  wavetableA: wavetable,
  wavetableB: wavetable,
  parameters: [{ id: "masterGain", normalized: 0.5 }],
};

test("accepts one complete bounded native snapshot", () => {
  assert.notEqual(parseUiSnapshot(valid), null);
});

test("rejects future schemas, oversize visuals, non-finite values, and duplicate parameters", () => {
  assert.equal(parseUiSnapshot({ ...valid, schemaVersion: 2 }), null);
  assert.equal(parseUiSnapshot({ ...valid, wavetableA: { ...wavetable, frameCount: 17 } }), null);
  assert.equal(parseUiSnapshot({ ...valid, activeVoices: Number.NaN }), null);
  assert.equal(parseUiSnapshot({ ...valid, parameters: [valid.parameters[0], valid.parameters[0]] }), null);
  assert.notEqual(parseUiSnapshot({ ...valid, modulationRouteCount: 1,
    modulationRoutes: [{ source: 2, destination: 0, amount: -0.5, curve: 2, enabled: true }] }), null);
  assert.equal(parseUiSnapshot({ ...valid, modulationRouteCount: 1, modulationRoutes: [] }), null);
  assert.equal(parseUiSnapshot({ ...valid, modulationRouteCount: 1,
    modulationRoutes: [{ source: 10, destination: 0, amount: 0, curve: 0, enabled: true }] }), null);
});

test("animation policy pauses hidden views and honors accessibility fallbacks", () => {
  assert.deepEqual(chooseAnimationPolicy({ visible: false, lowGraphics: false, reducedMotion: false }),
    { mode: "paused", framesPerSecond: 0 });
  assert.deepEqual(chooseAnimationPolicy({ visible: true, lowGraphics: false, reducedMotion: true }),
    { mode: "static", framesPerSecond: 0 });
  assert.deepEqual(chooseAnimationPolicy({ visible: true, lowGraphics: true, reducedMotion: false }),
    { mode: "canvas2d", framesPerSecond: 12 });
  assert.deepEqual(chooseAnimationPolicy({ visible: true, lowGraphics: false, reducedMotion: false }),
    { mode: "three", framesPerSecond: 30 });
});
