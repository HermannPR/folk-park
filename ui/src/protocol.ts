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
  wavetableA: WavetableSnapshot;
  wavetableB: WavetableSnapshot;
  parameters: ParameterSnapshot[];
};

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
  if (wavetableA === null || wavetableB === null || composition === null) return null;
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
  return { ...value, wavetableA, wavetableB, composition, modulationRoutes, parameters } as UiSnapshot;
}

export function chooseAnimationPolicy(preferences: GraphicsPreferences): AnimationPolicy {
  if (!preferences.visible) return { mode: "paused", framesPerSecond: 0 };
  if (preferences.reducedMotion) return { mode: "static", framesPerSecond: 0 };
  if (preferences.lowGraphics) return { mode: "canvas2d", framesPerSecond: 12 };
  return { mode: "three", framesPerSecond: 30 };
}
