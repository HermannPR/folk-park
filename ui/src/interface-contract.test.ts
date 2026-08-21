import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";
import { resolve } from "node:path";

const sourceRoot = import.meta.dirname;

test("production information architecture and accessibility surfaces remain explicit", async () => {
  const app = await readFile(resolve(sourceRoot, "App.tsx"), "utf8");
  for (const tab of ["SYNTH", "COMPOSE", "FX", "HISTORY", "SETTINGS"])
    assert.match(app, new RegExp(`"${tab}"`));
  assert.match(app, /aria-label="Primary"/);
  assert.match(app, /aria-current=/);
  assert.match(app, /aria-live="polite"/);
  assert.match(app, /ArrowLeft/);
  assert.match(app, /ArrowRight/);
  assert.match(app, /Low Graphics/);
  assert.match(app, /Reduced Motion/);
});

test("visualizer includes actual bounded Three scene and 2D fallback without external assets", async () => {
  const visualizer = await readFile(resolve(sourceRoot, "WavetableVisual.tsx"), "utf8");
  assert.match(visualizer, /THREE\.WebGLRenderer/);
  assert.match(visualizer, /drawCanvas2d/);
  assert.match(visualizer, /document\.visibilityState/);
  assert.match(visualizer, /ResizeObserver/);
  assert.doesNotMatch(visualizer, /https?:\/\//);
});

test("audition keyboard exposes touch, computer-key, accessibility, and release safety", async () => {
  const keyboard = await readFile(resolve(sourceRoot, "PianoKeyboard.tsx"), "utf8");
  assert.match(keyboard, /previewNoteOn/);
  assert.match(keyboard, /previewNoteOff/);
  assert.match(keyboard, /releasePreviewNotes/);
  assert.match(keyboard, /onPointerCancel/);
  assert.match(keyboard, /event\.repeat/);
  assert.match(keyboard, /noteSources\.has\(source\)/);
  assert.match(keyboard, /window\.addEventListener\("blur"/);
  assert.match(keyboard, /document\.addEventListener\("visibilitychange"/);
  assert.match(keyboard, /aria-label=/);
  assert.match(keyboard, /isTypingTarget/);
  assert.match(keyboard, /firstNote = 36/);
  assert.match(keyboard, /lastNote = 83/);
  assert.match(keyboard, /Four-octave playable piano keyboard/);
  assert.match(keyboard, /setComputerBase/);
  assert.doesNotMatch(keyboard, /https?:\/\//);
});

test("composition editing and the 32-route modulation workspace stay explicit and bounded", async () => {
  const app = await readFile(resolve(sourceRoot, "App.tsx"), "utf8");
  const modulation = await readFile(resolve(sourceRoot, "ModulationPanel.tsx"), "utf8");
  assert.match(app, /editCompositionNote/);
  assert.match(app, /acceptance will be required again/);
  assert.match(app, /previous accepted version/);
  assert.match(modulation, /setModulationRoutes/);
  assert.match(modulation, /routes\.length >= 32/);
  assert.match(modulation, /Apply reviewed matrix/);
  assert.match(modulation, /Discard changes/);
  assert.doesNotMatch(modulation, /https?:\/\//);
});
