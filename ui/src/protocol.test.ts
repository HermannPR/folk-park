import assert from "node:assert/strict";
import test from "node:test";
import { chooseAnimationPolicy, parseHistoryComparison, parsePersistenceWorkspace,
  parseUiSnapshot, wavetableSamplesPerFrame } from "./protocol.ts";

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
  renderStatus: "idle",
  renderMessage: "No accepted WAV has been rendered",
  renderDestination: "",
  renderDuration: 0,
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

const idA = "8ca1788c-080f-4ea0-80a8-d9381084aa20";
const idB = "6b9cb569-45ec-49d3-9805-dbcde3417f41";
const persistenceStatus = { enabled: true, presetAvailable: true, historyAvailable: true,
  message: "ready", currentPresetId: idA, currentPresetName: "Lead",
  currentPresetDirty: false, retentionDays: 180, missingAssets: [] };

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

test("M6 persistence and comparison payloads are bounded before UI state publication", () => {
  const workspace = { ok: true, status: persistenceStatus, presetError: "", historyError: "",
    presets: [{ id: idA, name: "Lead", author: "Producer", tags: ["lead"], genre: "house",
      emotion: "bright", favorite: false, missingAssets: false,
      fileName: "Lead.folkparkpreset" }],
    history: [{ id: idB, parentId: "", createdUnixMs: 1, updatedUnixMs: 1,
      generatorVersion: "1.0.0-m3", promptSummary: "", presetId: idA,
      favorite: false, tags: ["house", "bright"], deleted: false }] };
  assert.notEqual(parsePersistenceWorkspace(workspace), null);
  assert.equal(parsePersistenceWorkspace({ ...workspace,
    status: { ...persistenceStatus, missingAssets: [{ slot: "oscillatorA", displayName: "x.wav",
      sha256: "not-a-hash", byteSize: 100 }] } }), null);
  assert.equal(parsePersistenceWorkspace({ ...workspace,
    presets: [...workspace.presets, { ...workspace.presets[0], id: "../unsafe" }] }), null);

  const detail = { id: idA, parentId: "", createdUnixMs: 1,
    generatorVersion: "1.0.0-m3", presetId: idA, favorite: false, tags: ["lead"],
    seed: 42, key: "D", scale: "natural_minor", tempoBpm: 124, bars: 4,
    genre: "house", emotion: "bright", clipCount: 4, noteCount: 120 };
  assert.notEqual(parseHistoryComparison({ first: detail, second: { ...detail, id: idB } }), null);
  assert.equal(parseHistoryComparison({ first: detail, second: detail }), null);
});
