import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";
import { resolve } from "node:path";

const sourceRoot = import.meta.dirname;

test("production information architecture and accessibility surfaces remain explicit", async () => {
  const app = await readFile(resolve(sourceRoot, "App.tsx"), "utf8");
  for (const tab of ["SYNTH", "COMPOSE", "JARVIS", "FX", "HISTORY", "SETTINGS"])
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

test("M5 exposes the complete ordered effect chain and isolated accepted WAV action", async () => {
  const app = await readFile(resolve(sourceRoot, "App.tsx"), "utf8");
  const controls = await readFile(resolve(sourceRoot, "host-controls.tsx"), "utf8");
  for (const relay of ["distBypass", "chorusBypass", "delayBypass", "reverbBypass",
    "compBypass", "eqBypass"])
    assert.match(app, new RegExp(`relay="${relay}"`));
  assert.match(app, /Render accepted WAV/);
  assert.match(app, /publishComposition\(parsed\)/);
  assert.match(app, /separate offline synth and effect chain/);
  assert.match(app, /never resets or seeks/);
  assert.match(controls, /getToggleState/);
  assert.doesNotMatch(app, /Effects workspace is prepared/);
});

test("M6 exposes native preset recovery and transactional composition history", async () => {
  const app = await readFile(resolve(sourceRoot, "App.tsx"), "utf8");
  const view = await readFile(resolve(sourceRoot, "PersistenceView.tsx"), "utf8");
  const editor = await readFile(resolve(sourceRoot, "../../src/plugin/PluginEditor.cpp"), "utf8");
  for (const command of ["getPersistenceWorkspace", "savePreset", "loadPreset",
    "choosePresetFile", "relinkPresetAsset", "setPresetFavorite", "recallHistory",
    "setHistoryFavorite", "setHistoryDeleted", "compareHistory",
    "setHistoryRetention", "cleanupHistory"])
    assert.match(editor, new RegExp(`withNativeFunction\\("${command}"`));
  assert.match(app, /<PersistenceView/);
  assert.match(view, /Missing preset assets/);
  assert.match(view, /Exact hash required/);
  assert.match(view, /Compare selected/);
  assert.match(view, /recoverable trash/);
  assert.match(view, /window\.confirm/);
  assert.match(view, /Save as new preset/);
  assert.match(view, /new stable UUID/);
  assert.match(view, /Explicitly replace current library preset/);
  assert.match(editor, /permanently removes only expired non-favorites/);
  assert.match(view, /Composition acceptance and audio remain available/);
  assert.doesNotMatch(app, /History and preset persistence are prepared/);
  assert.doesNotMatch(view, /https?:\/\//);
});

test("M7 exposes a bounded offline Jarvis conversation and explicit review boundaries", async () => {
  const app = await readFile(resolve(sourceRoot, "App.tsx"), "utf8");
  const view = await readFile(resolve(sourceRoot, "JarvisView.tsx"), "utf8");
  const editor = await readFile(resolve(sourceRoot, "../../src/plugin/PluginEditor.cpp"), "utf8");
  for (const command of ["getJarvisState", "getJarvisQuestions",
    "createJarvisSoundProposal", "auditionJarvisSide", "acceptJarvisProposal",
    "rejectJarvisProposal", "createJarvisComposition"])
    assert.match(editor, new RegExp(`withNativeFunction\\("${command}"`));
  assert.match(app, /<JarvisView/);
  assert.match(app, /Ask Jarvis/);
  assert.match(view, /Walk me through it/);
  assert.match(view, /No API key, network request, hidden edit, or automatic acceptance/);
  assert.match(view, /not a general-purpose LLM/);
  assert.match(view, /A · Original/);
  assert.match(view, /B · Proposal/);
  assert.match(view, /Accept B/);
  assert.match(view, /Reject and restore A/);
  assert.match(view, /Review piano roll before accepting/);
  const answerHandler = view.match(/const setAnswer =[\s\S]*?\n  };/)?.[0] ?? "";
  assert.doesNotMatch(answerHandler, /setProgress/,
    "typing one guided answer must not unmount the current two-question step");
  assert.match(editor, /hasOnlyObjectProperties/);
  assert.match(editor, /requiresExplicitAcceptance/);
  assert.doesNotMatch(view, /https?:\/\//);
});
