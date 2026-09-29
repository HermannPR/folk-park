export const uiSchemaVersion = 1 as const;
export const maximumWavetableFrames = 16;
export const wavetableSamplesPerFrame = 96;
export const maximumParameters = 256;

export type WavetableSnapshot = {
  frameCount: number;
  samplesPerFrame: number;
  samples: number[];
};

export type ParameterSnapshot = {
  id: string;
  normalized: number;
};

export type ModulationRouteSnapshot = {
  source: number;
  destination: number;
  amount: number;
  curve: number;
  enabled: boolean;
};

export type CompositionSnapshot = {
  status: string;
  hasCandidate: boolean;
  hasAccepted: boolean;
  candidateMatchesAccepted: boolean;
  candidateNotes: number;
  acceptedNotes: number;
  notes: PreviewNote[];
};

export type PreviewNote = {
  part: "chords" | "melody" | "bass" | "arp";
  start: number;
  duration: number;
  pitch: number;
  velocity: number;
};

export type UiSnapshot = {
  schemaVersion: 1;
  product: "folk park";
  version: string;
  architecture: "x86_64";
  activeVoices: number;
  importStatus: string;
  importMessage: string;
  renderStatus: string;
  renderMessage: string;
  renderDestination: string;
  renderDuration: number;
  modulationRouteCount: number;
  modulationRoutes: ModulationRouteSnapshot[];
  composition: CompositionSnapshot;
  persistence: PersistenceStatus;
  wavetableA: WavetableSnapshot;
  wavetableB: WavetableSnapshot;
  parameters: ParameterSnapshot[];
};

export type MissingAsset = {
  slot: "oscillatorA" | "oscillatorB";
  displayName: string;
  sha256: string;
  byteSize: number;
};

export type PersistenceStatus = {
  enabled: boolean;
  presetAvailable: boolean;
  historyAvailable: boolean;
  message: string;
  currentPresetId: string;
  currentPresetName: string;
  currentPresetDirty: boolean;
  retentionDays: number;
  missingAssets: MissingAsset[];
};

export type PresetLibrarySummary = {
  id: string;
  name: string;
  author: string;
  tags: string[];
  genre: string;
  emotion: string;
  favorite: boolean;
  missingAssets: boolean;
  fileName: string;
};

export type HistorySummary = {
  id: string;
  parentId: string;
  createdUnixMs: number;
  updatedUnixMs: number;
  generatorVersion: string;
  promptSummary: string;
  presetId: string;
  favorite: boolean;
  tags: string[];
  deleted: boolean;
};

export type PersistenceWorkspace = {
  ok: boolean;
  status: PersistenceStatus;
  presetError: string;
  historyError: string;
  presets: PresetLibrarySummary[];
  history: HistorySummary[];
};

export type HistoryDetail = {
  id: string;
  parentId: string;
  createdUnixMs: number;
  generatorVersion: string;
  presetId: string;
  favorite: boolean;
  tags: string[];
  seed: number;
  key: string;
  scale: string;
  tempoBpm: number;
  bars: number;
  genre: string;
  emotion: string;
  clipCount: number;
  noteCount: number;
};

export type HistoryComparison = { first: HistoryDetail; second: HistoryDetail };

export type GraphicsPreferences = {
  visible: boolean;
  lowGraphics: boolean;
  reducedMotion: boolean;
};

export type AnimationPolicy = {
  mode: "paused" | "static" | "canvas2d" | "three";
  framesPerSecond: number;
};

const isRecord = (value: unknown): value is Record<string, unknown> =>
  typeof value === "object" && value !== null && !Array.isArray(value);

const isFiniteRange = (value: unknown, minimum: number, maximum: number): value is number =>
  typeof value === "number" && Number.isFinite(value) && value >= minimum && value <= maximum;

const isBoundedString = (value: unknown, maximum: number): value is string =>
  typeof value === "string" && value.length <= maximum;

const uuidPattern = /^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;
const sha256Pattern = /^[0-9a-f]{64}$/;

function parseTags(value: unknown): string[] | null {
  if (!Array.isArray(value) || value.length > 24) return null;
  const tags: string[] = [];
  const unique = new Set<string>();
  for (const tag of value) {
    if (!isBoundedString(tag, 48) || tag.length === 0 || unique.has(tag)) return null;
    unique.add(tag); tags.push(tag);
  }
  return tags;
}

export function parsePersistenceStatus(value: unknown): PersistenceStatus | null {
  if (!isRecord(value)
      || typeof value.enabled !== "boolean"
      || typeof value.presetAvailable !== "boolean"
      || typeof value.historyAvailable !== "boolean"
      || !isBoundedString(value.message, 512)
      || !isBoundedString(value.currentPresetId, 64)
      || (value.currentPresetId.length > 0 && !uuidPattern.test(value.currentPresetId))
      || !isBoundedString(value.currentPresetName, 96)
      || typeof value.currentPresetDirty !== "boolean"
      || !Number.isInteger(value.retentionDays)
      || !isFiniteRange(value.retentionDays, 1, 3650)
      || !Array.isArray(value.missingAssets)
      || value.missingAssets.length > 2) return null;
  const missingAssets: MissingAsset[] = [];
  for (const asset of value.missingAssets) {
    if (!isRecord(asset)
        || !["oscillatorA", "oscillatorB"].includes(String(asset.slot))
        || !isBoundedString(asset.displayName, 128) || asset.displayName.length === 0
        || typeof asset.sha256 !== "string" || !sha256Pattern.test(asset.sha256)
        || !Number.isInteger(asset.byteSize)
        || !isFiniteRange(asset.byteSize, 1, 64 * 1024 * 1024)) return null;
    missingAssets.push(asset as MissingAsset);
  }
  return { ...value, missingAssets } as PersistenceStatus;
}

function parsePresetSummary(value: unknown): PresetLibrarySummary | null {
  if (!isRecord(value) || typeof value.id !== "string" || !uuidPattern.test(value.id)
      || !isBoundedString(value.name, 96) || value.name.length === 0
      || !isBoundedString(value.author, 96)
      || !isBoundedString(value.genre, 64) || !isBoundedString(value.emotion, 64)
      || typeof value.favorite !== "boolean" || typeof value.missingAssets !== "boolean"
      || !isBoundedString(value.fileName, 96) || !value.fileName.endsWith(".folkparkpreset")) return null;
  const tags = parseTags(value.tags);
  return tags === null ? null : { ...value, tags } as PresetLibrarySummary;
}

function parseHistorySummary(value: unknown): HistorySummary | null {
  if (!isRecord(value) || typeof value.id !== "string" || !uuidPattern.test(value.id)
      || !isBoundedString(value.parentId, 64)
      || (value.parentId.length > 0 && !uuidPattern.test(value.parentId))
      || !Number.isInteger(value.createdUnixMs) || !isFiniteRange(value.createdUnixMs, 0, 9e15)
      || !Number.isInteger(value.updatedUnixMs) || !isFiniteRange(value.updatedUnixMs, 0, 9e15)
      || !isBoundedString(value.generatorVersion, 32)
      || !isBoundedString(value.promptSummary, 512)
      || !isBoundedString(value.presetId, 64)
      || (value.presetId.length > 0 && !uuidPattern.test(value.presetId))
      || typeof value.favorite !== "boolean" || typeof value.deleted !== "boolean") return null;
  const tags = parseTags(value.tags);
  return tags === null ? null : { ...value, tags } as HistorySummary;
}

export function parsePersistenceWorkspace(value: unknown): PersistenceWorkspace | null {
  if (!isRecord(value) || typeof value.ok !== "boolean"
      || !isBoundedString(value.presetError, 512)
      || !isBoundedString(value.historyError, 512)
      || !Array.isArray(value.presets) || value.presets.length > 512
      || !Array.isArray(value.history) || value.history.length > 100) return null;
  const status = parsePersistenceStatus(value.status);
  const presets = value.presets.map(parsePresetSummary);
  const history = value.history.map(parseHistorySummary);
  if (status === null || presets.some((entry) => entry === null)
      || history.some((entry) => entry === null)) return null;
  return { ok: value.ok, status, presetError: value.presetError,
    historyError: value.historyError, presets: presets as PresetLibrarySummary[],
    history: history as HistorySummary[] };
}

function parseHistoryDetail(value: unknown): HistoryDetail | null {
  if (!isRecord(value) || typeof value.id !== "string" || !uuidPattern.test(value.id)
      || !isBoundedString(value.parentId, 64)
      || (value.parentId.length > 0 && !uuidPattern.test(value.parentId))
      || !Number.isInteger(value.createdUnixMs) || !isFiniteRange(value.createdUnixMs, 0, 9e15)
      || !isBoundedString(value.generatorVersion, 32)
      || !isBoundedString(value.presetId, 64)
      || (value.presetId.length > 0 && !uuidPattern.test(value.presetId))
      || typeof value.favorite !== "boolean" || !Number.isInteger(value.seed)
      || !isFiniteRange(value.seed, 0, 4294967295)
      || !isBoundedString(value.key, 16) || !isBoundedString(value.scale, 32)
      || !isFiniteRange(value.tempoBpm, 20, 400) || !Number.isInteger(value.bars)
      || !isFiniteRange(value.bars, 1, 64) || !isBoundedString(value.genre, 64)
      || !isBoundedString(value.emotion, 64) || !Number.isInteger(value.clipCount)
      || !isFiniteRange(value.clipCount, 1, 4) || !Number.isInteger(value.noteCount)
      || !isFiniteRange(value.noteCount, 0, 16384)) return null;
  const tags = parseTags(value.tags);
  return tags === null ? null : { ...value, tags } as HistoryDetail;
}

export function parseHistoryComparison(value: unknown): HistoryComparison | null {
  if (!isRecord(value)) return null;
  const first = parseHistoryDetail(value.first);
  const second = parseHistoryDetail(value.second);
  return first === null || second === null || first.id === second.id ? null : { first, second };
}

function parseWavetable(value: unknown): WavetableSnapshot | null {
  if (!isRecord(value)
      || !Number.isInteger(value.frameCount)
      || !isFiniteRange(value.frameCount, 1, maximumWavetableFrames)
      || value.samplesPerFrame !== wavetableSamplesPerFrame
      || !Array.isArray(value.samples)
      || value.samples.length !== value.frameCount * wavetableSamplesPerFrame) {
    return null;
  }
  if (!value.samples.every((sample) => isFiniteRange(sample, -1.0001, 1.0001))) return null;
  return {
    frameCount: value.frameCount,
    samplesPerFrame: value.samplesPerFrame,
    samples: [...value.samples],
  };
}

function parsePreviewNote(value: unknown): PreviewNote | null {
  if (!isRecord(value)
      || !["chords", "melody", "bass", "arp"].includes(String(value.part))
      || !isFiniteRange(value.start, 0, 1)
      || !isFiniteRange(value.duration, 0.000001, 1)
      || !isFiniteRange(value.pitch, 0, 1)
      || !isFiniteRange(value.velocity, 0, 1)) return null;
  return value as PreviewNote;
}

export function parseComposition(value: unknown): CompositionSnapshot | null {
  if (!isRecord(value)
      || !isBoundedString(value.status, 512)
      || typeof value.hasCandidate !== "boolean"
      || typeof value.hasAccepted !== "boolean"
      || typeof value.candidateMatchesAccepted !== "boolean"
      || !isFiniteRange(value.candidateNotes, 0, 16384)
      || !isFiniteRange(value.acceptedNotes, 0, 16384)
      || !Array.isArray(value.notes)
      || value.notes.length > 4096) return null;
  const notes = value.notes.map(parsePreviewNote);
  if (notes.some((note) => note === null)) return null;
  return {
    status: value.status,
    hasCandidate: value.hasCandidate,
    hasAccepted: value.hasAccepted,
    candidateMatchesAccepted: value.candidateMatchesAccepted,
    candidateNotes: value.candidateNotes,
    acceptedNotes: value.acceptedNotes,
    notes: notes as PreviewNote[],
  };
}

export function parseUiSnapshot(value: unknown): UiSnapshot | null {
  if (!isRecord(value)
      || value.schemaVersion !== uiSchemaVersion
      || value.product !== "folk park"
      || value.architecture !== "x86_64"
      || !isBoundedString(value.version, 32)
      || !isFiniteRange(value.activeVoices, 0, 16)
      || !isBoundedString(value.importStatus, 64)
      || !isBoundedString(value.importMessage, 512)
      || !isBoundedString(value.renderStatus, 64)
      || !isBoundedString(value.renderMessage, 512)
      || !isBoundedString(value.renderDestination, 2048)
      || !isFiniteRange(value.renderDuration, 0, 900)
      || !isFiniteRange(value.modulationRouteCount, 0, 32)
      || !Array.isArray(value.modulationRoutes)
      || value.modulationRoutes.length !== value.modulationRouteCount
      || !Array.isArray(value.parameters)
      || value.parameters.length > maximumParameters) return null;
  const wavetableA = parseWavetable(value.wavetableA);
  const wavetableB = parseWavetable(value.wavetableB);
  const composition = parseComposition(value.composition);
  const persistence = value.persistence === undefined
    ? { enabled: false, presetAvailable: false, historyAvailable: false,
      message: "Persistence status was not supplied by this compatible snapshot",
      currentPresetId: "", currentPresetName: "Init / session", currentPresetDirty: false,
      retentionDays: 180, missingAssets: [] } satisfies PersistenceStatus
    : parsePersistenceStatus(value.persistence);
  if (wavetableA === null || wavetableB === null || composition === null
      || persistence === null) return null;
  const parameters: ParameterSnapshot[] = [];
  const modulationRoutes: ModulationRouteSnapshot[] = [];
  for (const route of value.modulationRoutes) {
    if (!isRecord(route) || !Number.isInteger(route.source) || !isFiniteRange(route.source, 0, 9)
        || !Number.isInteger(route.destination) || !isFiniteRange(route.destination, 0, 12)
        || !isFiniteRange(route.amount, -1, 1) || !Number.isInteger(route.curve)
        || !isFiniteRange(route.curve, 0, 2) || typeof route.enabled !== "boolean") return null;
    modulationRoutes.push(route as ModulationRouteSnapshot);
  }
  const ids = new Set<string>();
  for (const entry of value.parameters) {
    if (!isRecord(entry) || !isBoundedString(entry.id, 64) || entry.id.length === 0
        || !isFiniteRange(entry.normalized, 0, 1) || ids.has(entry.id)) return null;
    ids.add(entry.id);
    parameters.push({ id: entry.id, normalized: entry.normalized });
  }
  return { ...value, wavetableA, wavetableB, composition, persistence,
    modulationRoutes, parameters } as UiSnapshot;
}

export function chooseAnimationPolicy(preferences: GraphicsPreferences): AnimationPolicy {
  if (!preferences.visible) return { mode: "paused", framesPerSecond: 0 };
  if (preferences.reducedMotion) return { mode: "static", framesPerSecond: 0 };
  if (preferences.lowGraphics) return { mode: "canvas2d", framesPerSecond: 12 };
  return { mode: "three", framesPerSecond: 30 };
}
