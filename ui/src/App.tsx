import { useCallback, useEffect, useState } from "react";
import { getNativeFunction } from "@juce/index.js";
import { AssistantProviderSettings } from "./AssistantProviderSettings.tsx";
import { HostCombo, HostSlider, HostToggle, useHostNormalized } from "./host-controls.tsx";
import { JarvisView } from "./JarvisView.tsx";
import { PianoKeyboard } from "./PianoKeyboard.tsx";
import { ModulationPanel } from "./ModulationPanel.tsx";
import { PersistenceView } from "./PersistenceView.tsx";
import { parseComposition, parsePersistenceStatus, parseUiSnapshot,
  type CompositionSnapshot, type UiSnapshot } from "./protocol.ts";
import { WavetableVisual } from "./WavetableVisual.tsx";

const tabs = ["SYNTH", "COMPOSE", "JARVIS", "FX", "HISTORY", "SETTINGS"] as const;
type Tab = typeof tabs[number];

const native = {
  getUiSnapshot: getNativeFunction("getUiSnapshot"),
  panic: getNativeFunction("panic"),
  undo: getNativeFunction("undo"),
  redo: getNativeFunction("redo"),
  chooseWavetable: getNativeFunction("chooseWavetable"),
  confirmWavetableImport: getNativeFunction("confirmWavetableImport"),
  cancelWavetableImport: getNativeFunction("cancelWavetableImport"),
  generateComposition: getNativeFunction("generateComposition"),
  moreLikeComposition: getNativeFunction("moreLikeComposition"),
  surpriseComposition: getNativeFunction("surpriseComposition"),
  editCompositionNote: getNativeFunction("editCompositionNote"),
  acceptComposition: getNativeFunction("acceptComposition"),
  exportAcceptedMidi: getNativeFunction("exportAcceptedMidi"),
  routeAcceptedMidi: getNativeFunction("routeAcceptedMidi"),
  stopDirectMidi: getNativeFunction("stopDirectMidi"),
  renderAcceptedWav: getNativeFunction("renderAcceptedWav"),
  cancelAcceptedWav: getNativeFunction("cancelAcceptedWav"),
};

type Preferences = { lowGraphics: boolean; reducedMotion: boolean };

function StatusPill({ children, good = false }: { children: React.ReactNode; good?: boolean }) {
  return <span className={`status-pill ${good ? "good" : ""}`}><i />{children}</span>;
}

function SynthView({ snapshot, visible, preferences, announce, refresh, publishSnapshot }:
  { snapshot: UiSnapshot; visible: boolean; preferences: Preferences;
    announce: (message: string) => void; refresh: () => Promise<void>;
    publishSnapshot: (snapshot: UiSnapshot) => void }) {
  const importFor = async (oscillator: number) => announce(String(await native.chooseWavetable(oscillator)));
  const confirm = async () => { announce(String(await native.confirmWavetableImport())); await refresh(); };
  const oscillatorAPosition = useHostNormalized("oscAPosition");
  const oscillatorBPosition = useHostNormalized("oscBPosition");
  return <div className="view-grid synth-view">
    <div className="visual-pair wide-card">
      <WavetableVisual name="A" table={snapshot.wavetableA}
        position={oscillatorAPosition} routeActive={snapshot.modulationRouteCount > 0}
        visible={visible} {...preferences} />
      <WavetableVisual name="B" table={snapshot.wavetableB}
        position={oscillatorBPosition} routeActive={snapshot.modulationRouteCount > 0}
        visible={visible} {...preferences} />
    </div>
    <PianoKeyboard announce={announce} />
    <section className="surface">
      <div className="section-heading"><div><span>01</span><h2>Oscillators</h2></div><small>Host-aware</small></div>
      <div className="control-grid">
        <HostCombo id="waveform" relay="oscWaveform" label="Legacy A shape" />
        <HostSlider id="osc-a-position" relay="oscAPosition" label="A position" decimals={3} />
        <HostSlider id="osc-b-position" relay="oscBPosition" label="B position" decimals={3} />
      </div>
    </section>
    <section className="surface">
      <div className="section-heading"><div><span>02</span><h2>Mixer</h2></div><small>Osc · sub · noise</small></div>
      <div className="control-grid">
        <HostSlider id="osc-a-level" relay="oscLevel" label="A level" decimals={1} />
        <HostSlider id="osc-b-level" relay="oscBLevel" label="B level" decimals={1} />
        <HostCombo id="sub-waveform" relay="subWaveform" label="Sub shape" />
        <HostSlider id="sub-level" relay="subLevel" label="Sub level" decimals={1} />
        <HostCombo id="noise-type" relay="noiseType" label="Noise type" />
        <HostSlider id="noise-level" relay="noiseLevel" label="Noise level" decimals={1} />
        <HostSlider id="master" relay="masterGain" label="Master" decimals={1} />
      </div>
    </section>
    <section className="surface wide-card">
      <div className="section-heading"><div><span>03</span><h2>Filter</h2></div><small>Real-time safe</small></div>
      <div className="control-grid">
        <HostCombo id="filter-mode" relay="filterMode" label="Mode" />
        <HostSlider id="cutoff" relay="filterCutoff" label="Cutoff" decimals={0} />
        <HostSlider id="resonance" relay="filterResonance" label="Resonance" decimals={3} />
        <HostSlider id="drive" relay="filterDrive" label="Drive" decimals={1} />
        <HostSlider id="filter-key" relay="filterKeyTracking" label="Key tracking" decimals={3} />
        <HostSlider id="filter-env-amount" relay="filterEnvAmount" label="Envelope amount" decimals={3} />
      </div>
    </section>
    <section className="surface wide-card">
      <div className="section-heading"><div><span>04</span><h2>Envelopes</h2></div><small>Amp · filter · auxiliary</small></div>
      <div className="envelope-grid">
        <div><h3>Amp</h3><HostSlider id="amp-a" relay="ampAttack" label="Attack" decimals={3} /><HostSlider id="amp-d" relay="ampDecay" label="Decay" decimals={3} /><HostSlider id="amp-s" relay="ampSustain" label="Sustain" decimals={3} /><HostSlider id="amp-r" relay="ampRelease" label="Release" decimals={3} /></div>
        <div><h3>Filter</h3><HostSlider id="fenv-a" relay="filterEnvAttack" label="Attack" decimals={3} /><HostSlider id="fenv-d" relay="filterEnvDecay" label="Decay" decimals={3} /><HostSlider id="fenv-s" relay="filterEnvSustain" label="Sustain" decimals={3} /><HostSlider id="fenv-r" relay="filterEnvRelease" label="Release" decimals={3} /></div>
        <div><h3>Auxiliary</h3><HostSlider id="aenv-a" relay="auxEnvAttack" label="Attack" decimals={3} /><HostSlider id="aenv-d" relay="auxEnvDecay" label="Decay" decimals={3} /><HostSlider id="aenv-s" relay="auxEnvSustain" label="Sustain" decimals={3} /><HostSlider id="aenv-r" relay="auxEnvRelease" label="Release" decimals={3} /></div>
      </div>
    </section>
    <section className="surface wide-card">
      <div className="section-heading"><div><span>05</span><h2>LFOs</h2></div><small>Four host-synced sources</small></div>
      <div className="lfo-grid">{[1,2,3,4].map((number) => <div key={number}><h3>LFO {number}</h3><HostCombo id={`lfo-${number}-shape`} relay={`lfo${number}Shape`} label="Shape" /><HostSlider id={`lfo-${number}-rate`} relay={`lfo${number}Rate`} label="Rate" decimals={2} /></div>)}</div>
    </section>
    <ModulationPanel initial={snapshot.modulationRoutes} onSnapshot={publishSnapshot} announce={announce} />
    <section className="surface wide-card import-card">
      <div className="section-heading"><div><span>07</span><h2>Reviewed wavetable import</h2></div><small>{snapshot.importStatus}</small></div>
      <p>{snapshot.importMessage}</p>
      <div className="actions"><button onClick={() => void importFor(0)}>Review WAV for A</button><button onClick={() => void importFor(1)}>Review WAV for B</button><button className="primary" onClick={() => void confirm()}>Confirm</button><button onClick={() => void native.cancelWavetableImport().then((value) => announce(String(value)))}>Cancel</button></div>
    </section>
  </div>;
}

const macroIds = ["density", "rhythm", "tension", "human", "repeat", "variation"] as const;

function ComposeView({ initial, announce, publishComposition }:
  { initial: CompositionSnapshot; announce: (message: string) => void;
    publishComposition: (composition: CompositionSnapshot) => void }) {
  const [composition, setComposition] = useState(initial);
  const [seed, setSeed] = useState(12345);
  const [key, setKey] = useState("C");
  const [scale, setScale] = useState("natural_minor");
  const [tempo, setTempo] = useState(124);
  const [bars, setBars] = useState(4);
  const [macros, setMacros] = useState([0.55, 0.45, 0.7, 0.12, 0.6, 0.4]);
  const [parts, setParts] = useState([true, true, true, true]);
  const [variationIndex, setVariationIndex] = useState(1);
  const [selectedNote, setSelectedNote] = useState<number | null>(null);
  const deliveryIsPrevious = composition.hasAccepted && !composition.candidateMatchesAccepted;
  const apply = (value: unknown) => {
    const parsed = parseComposition(value);
    if (parsed === null) announce(String(value));
    else { setComposition(parsed); publishComposition(parsed); }
  };
  const generate = async () => apply(await native.generateComposition(seed, key, scale, tempo, bars,
    ...macros, ...parts));
  const vary = async () => { apply(await native.moreLikeComposition(variationIndex)); setVariationIndex((value) => value + 1); };
  const surprise = async () => { apply(await native.surpriseComposition(variationIndex)); setVariationIndex((value) => value + 1); };
  const accept = async () => apply(await native.acceptComposition());
  const edit = async (pitch: number, start: number, duration: number, velocity: number) => {
    if (selectedNote === null) return;
    const sourceIndex = selectedNote;
    setSelectedNote(null);
    const result = await native.editCompositionNote(sourceIndex, pitch, start, duration, velocity);
    const parsed = parseComposition(result);
    if (parsed === null) { setSelectedNote(sourceIndex); announce(String(result)); }
    else { setComposition(parsed); announce(parsed.status); }
  };
  return <div className="compose-layout">
    <section className="surface intent-panel">
      <div className="section-heading"><div><span>01</span><h2>Musical intent</h2></div><small>Local deterministic</small></div>
      <div className="intent-grid">
        <label>Seed<input type="number" min="0" max="4294967295" value={seed} onChange={(event) => setSeed(Number(event.currentTarget.value))} /></label>
        <label>Key<select value={key} onChange={(event) => setKey(event.currentTarget.value)}>{["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"].map((value) => <option key={value}>{value}</option>)}</select></label>
        <label>Scale<select value={scale} onChange={(event) => setScale(event.currentTarget.value)}>{["major","natural_minor","harmonic_minor","dorian","mixolydian","pentatonic_major","pentatonic_minor"].map((value) => <option key={value} value={value}>{value.replaceAll("_", " ")}</option>)}</select></label>
        <label>BPM<input type="number" min="20" max="400" value={tempo} onChange={(event) => setTempo(Number(event.currentTarget.value))} /></label>
        <label>Bars<input type="number" min="1" max="64" value={bars} onChange={(event) => setBars(Number(event.currentTarget.value))} /></label>
      </div>
      <div className="macro-grid">{macroIds.map((id, index) => <label key={id}>{id}<output>{(macros[index] ?? 0).toFixed(2)}</output><input type="range" min="0" max="1" step="0.01" value={macros[index]} onChange={(event) => setMacros((values) => values.map((value, valueIndex) => valueIndex === index ? Number(event.currentTarget.value) : value))} /></label>)}</div>
      <div className="parts">{["Chords", "Melody", "Bass", "Arpeggio"].map((name, index) => <label key={name}><input type="checkbox" checked={parts[index]} onChange={(event) => setParts((values) => values.map((value, valueIndex) => valueIndex === index ? event.currentTarget.checked : value))} />{name}</label>)}</div>
      <div className="actions"><button className="primary" onClick={() => void generate()}>Generate candidate</button><button onClick={() => void vary()}>More Like This</button><button onClick={() => void surprise()}>Surprise Me</button><button className="accept" onClick={() => void accept()}>Accept</button></div>
    </section>
    <section className="surface preview-panel">
      <div className="section-heading"><div><span>02</span><h2>Piano roll preview</h2></div><small>{composition.candidateNotes} notes</small></div>
      <div className="piano-roll" role="group" aria-label="Bounded editable generated MIDI piano-roll preview">
        {composition.notes.map((note, index) => <button type="button" className={`${note.part} ${selectedNote === index ? "selected" : ""}`} key={`${note.part}-${index}`} style={{ left: `${note.start * 100}%`, width: `${Math.max(.2, note.duration * 100)}%`, bottom: `${note.pitch * 94}%`, opacity: .45 + note.velocity * .55 }} aria-label={`${note.part} note ${index + 1}`} aria-pressed={selectedNote === index} onClick={() => setSelectedNote(index)} onKeyDown={(event) => {
          if (!['ArrowUp','ArrowDown','ArrowLeft','ArrowRight'].includes(event.key)) return;
          event.preventDefault();
          if (event.key === 'ArrowUp') void edit(1, 0, 0, 0);
          if (event.key === 'ArrowDown') void edit(-1, 0, 0, 0);
          if (event.key === 'ArrowLeft') void edit(0, event.shiftKey ? 0 : -120, event.shiftKey ? -120 : 0, 0);
          if (event.key === 'ArrowRight') void edit(0, event.shiftKey ? 0 : 120, event.shiftKey ? 120 : 0, 0);
        }} />)}
      </div>
      <div className="note-editor" aria-label="Selected candidate note editor"><span>{selectedNote === null ? "Select a note to edit the candidate" : `Editing note ${selectedNote + 1} · acceptance will be required again`}</span><div><button disabled={selectedNote === null} onClick={() => void edit(-1, 0, 0, 0)}>Pitch −</button><button disabled={selectedNote === null} onClick={() => void edit(1, 0, 0, 0)}>Pitch +</button><button disabled={selectedNote === null} onClick={() => void edit(0, -120, 0, 0)}>Earlier</button><button disabled={selectedNote === null} onClick={() => void edit(0, 120, 0, 0)}>Later</button><button disabled={selectedNote === null} onClick={() => void edit(0, 0, -120, 0)}>Shorter</button><button disabled={selectedNote === null} onClick={() => void edit(0, 0, 120, 0)}>Longer</button><button disabled={selectedNote === null} onClick={() => void edit(0, 0, 0, -8)}>Softer</button><button disabled={selectedNote === null} onClick={() => void edit(0, 0, 0, 8)}>Louder</button></div></div>
      <p className="composition-status">{composition.status}{composition.hasAccepted && !composition.candidateMatchesAccepted ? " · Delivery still points to the previous accepted version." : ""}</p>
      <div className="delivery-grid"><button disabled={!composition.hasAccepted} onClick={() => void native.exportAcceptedMidi().then((value) => announce(String(value)))}>Export {deliveryIsPrevious ? "previous accepted" : "accepted"} MIDI</button><button disabled={!composition.hasAccepted} onClick={() => void native.routeAcceptedMidi().then((value) => announce(String(value)))}>Route {deliveryIsPrevious ? "previous accepted" : "accepted"} MIDI</button><button onClick={() => void native.stopDirectMidi().then((value) => announce(String(value)))}>Stop</button></div>
      <small>Arrow keys change a selected note's pitch or timing; Shift + Left/Right changes duration. Native drag below uses only the explicitly accepted bundle.</small>
    </section>
  </div>;
}

function FxView({ snapshot, announce }: { snapshot: UiSnapshot; announce: (message: string) => void }) {
  const busy = snapshot.renderStatus === "rendering";
  return <div className="fx-layout">
    <section className="surface effect-card">
      <div className="section-heading"><div><span>01</span><h2>Distortion</h2></div><small>First</small></div>
      <div className="effect-controls"><HostToggle id="dist-bypass" relay="distBypass" label="Distortion" /><HostSlider id="dist-drive" relay="distDrive" label="Drive" decimals={1} /><HostSlider id="dist-mix" relay="distMix" label="Mix" decimals={2} /><HostSlider id="dist-output" relay="distOutput" label="Output" decimals={1} /></div>
    </section>
    <section className="surface effect-card">
      <div className="section-heading"><div><span>02</span><h2>Chorus</h2></div><small>Stereo modulation</small></div>
      <div className="effect-controls"><HostToggle id="chorus-bypass" relay="chorusBypass" label="Chorus" /><HostSlider id="chorus-rate" relay="chorusRate" label="Rate" decimals={2} /><HostSlider id="chorus-depth" relay="chorusDepth" label="Depth" decimals={1} /><HostSlider id="chorus-mix" relay="chorusMix" label="Mix" decimals={2} /></div>
    </section>
    <section className="surface effect-card">
      <div className="section-heading"><div><span>03</span><h2>Delay</h2></div><small>Host tempo</small></div>
      <div className="effect-controls"><HostToggle id="delay-bypass" relay="delayBypass" label="Delay" /><HostCombo id="delay-division" relay="delayDivision" label="Division" /><HostSlider id="delay-feedback" relay="delayFeedback" label="Feedback" decimals={2} /><HostSlider id="delay-mix" relay="delayMix" label="Mix" decimals={2} /></div>
    </section>
    <section className="surface effect-card">
      <div className="section-heading"><div><span>04</span><h2>Reverb</h2></div><small>Space</small></div>
      <div className="effect-controls"><HostToggle id="reverb-bypass" relay="reverbBypass" label="Reverb" /><HostSlider id="reverb-room" relay="reverbRoomSize" label="Room size" decimals={2} /><HostSlider id="reverb-damping" relay="reverbDamping" label="Damping" decimals={2} /><HostSlider id="reverb-mix" relay="reverbMix" label="Mix" decimals={2} /></div>
    </section>
    <section className="surface effect-card">
      <div className="section-heading"><div><span>05</span><h2>Compressor</h2></div><small>Dynamics</small></div>
      <div className="effect-controls effect-controls-six"><HostToggle id="comp-bypass" relay="compBypass" label="Compressor" /><HostSlider id="comp-threshold" relay="compThreshold" label="Threshold" decimals={1} /><HostSlider id="comp-ratio" relay="compRatio" label="Ratio" decimals={1} /><HostSlider id="comp-attack" relay="compAttack" label="Attack" decimals={1} /><HostSlider id="comp-release" relay="compRelease" label="Release" decimals={0} /><HostSlider id="comp-makeup" relay="compMakeup" label="Makeup" decimals={1} /><HostSlider id="comp-mix" relay="compMix" label="Mix" decimals={2} /></div>
    </section>
    <section className="surface effect-card">
      <div className="section-heading"><div><span>06</span><h2>Parametric EQ</h2></div><small>Last</small></div>
      <div className="effect-controls effect-controls-six"><HostToggle id="eq-bypass" relay="eqBypass" label="EQ" /><HostSlider id="eq-low" relay="eqLowGain" label="Low shelf" decimals={1} /><HostSlider id="eq-mid-frequency" relay="eqMidFrequency" label="Mid frequency" decimals={0} /><HostSlider id="eq-mid-gain" relay="eqMidGain" label="Mid gain" decimals={1} /><HostSlider id="eq-mid-q" relay="eqMidQ" label="Mid Q" decimals={2} /><HostSlider id="eq-high" relay="eqHighGain" label="High shelf" decimals={1} /></div>
    </section>
    <section className="surface render-card">
      <div className="section-heading"><div><span>WAV</span><h2>Accepted composition preview</h2></div><small>{snapshot.renderStatus}</small></div>
      <p>{snapshot.renderMessage}</p>
      {snapshot.renderDestination && <code title={snapshot.renderDestination}>{snapshot.renderDestination}</code>}
      {snapshot.renderDuration > 0 && <small>{snapshot.renderDuration.toFixed(2)} seconds · 48 kHz · stereo 24-bit</small>}
      <div className="actions"><button className="primary" disabled={!snapshot.composition.hasAccepted || busy} onClick={() => void native.renderAcceptedWav().then((value) => announce(String(value)))}>Render accepted WAV</button><button disabled={!busy} onClick={() => void native.cancelAcceptedWav().then((value) => announce(String(value)))}>Cancel render</button></div>
      <p className="render-note">Uses a separate offline synth and effect chain. It never resets or seeks the voices currently playing in FL Studio.</p>
    </section>
  </div>;
}

export default function App() {
  const [tab, setTab] = useState<Tab>("SYNTH");
  const [snapshot, setSnapshot] = useState<UiSnapshot | null>(null);
  const [error, setError] = useState("Requesting complete native snapshot…");
  const [announcement, setAnnouncement] = useState("Native bridge starting");
  const [jarvisDraft, setJarvisDraft] = useState("");
  const [visible, setVisible] = useState(document.visibilityState === "visible");
  const [preferences, setPreferences] = useState<Preferences>({ lowGraphics: false,
    reducedMotion: window.matchMedia("(prefers-reduced-motion: reduce)").matches });

  const refresh = useCallback(async () => {
    const parsed = parseUiSnapshot(await native.getUiSnapshot());
    if (parsed === null) { setError("Rejected malformed or unsupported native snapshot"); return; }
    setSnapshot(parsed); setError(""); setAnnouncement("Complete native snapshot restored");
  }, []);
  useEffect(() => { void refresh(); }, [refresh]);
  useEffect(() => {
    const update = () => setVisible(document.visibilityState === "visible");
    document.addEventListener("visibilitychange", update);
    return () => document.removeEventListener("visibilitychange", update);
  }, []);
  useEffect(() => {
    const listener = window.__JUCE__.backend.addEventListener("processorSnapshot", (payload) => {
      if (typeof payload === "object" && payload !== null && "activeVoices" in payload) {
        const voices = Number((payload as { activeVoices: unknown }).activeVoices);
        if (Number.isFinite(voices) && voices >= 0 && voices <= 16)
          setSnapshot((current) => {
            if (current === null) return null;
            const update = payload as Record<string, unknown>;
            const renderStatus = typeof update.renderStatus === "string" ? update.renderStatus : current.renderStatus;
            const renderMessage = typeof update.renderMessage === "string" ? update.renderMessage : current.renderMessage;
            const renderDestination = typeof update.renderDestination === "string" ? update.renderDestination : current.renderDestination;
            const duration = Number(update.renderDuration);
            const renderDuration = Number.isFinite(duration) && duration >= 0 && duration <= 900 ? duration : current.renderDuration;
            const persistence = parsePersistenceStatus(update.persistence) ?? current.persistence;
            return { ...current, activeVoices: voices, renderStatus, renderMessage,
              renderDestination, renderDuration, persistence };
          });
      }
    });
    return () => window.__JUCE__.backend.removeEventListener?.("processorSnapshot", listener);
  }, []);
  const announce = useCallback((message: string) => setAnnouncement(message.slice(0, 512)), []);
  return <div className="app-shell">
    <div className="atmosphere" aria-hidden="true"><i /><i /><i /></div>
    <header className="app-header">
      <div className="brand"><span>Silicon Dreams</span><h1>folk park</h1><small>0.1 · M7 guided production assistant</small></div>
      <div className="preset-stack"><button className="preset" onClick={() => setTab("HISTORY")}><span>Current sound</span><strong>{snapshot?.persistence.currentPresetName ?? "Init / session"}{snapshot?.persistence.currentPresetDirty ? " *" : ""}</strong><i>⌄</i></button><label className="assistant-preview"><span>Ask Jarvis</span><input maxLength={1024} value={jarvisDraft} onFocus={() => setTab("JARVIS")} onChange={(event) => setJarvisDraft(event.currentTarget.value)} onKeyDown={(event) => { if (event.key === "Enter") { event.preventDefault(); setTab("JARVIS"); } }} placeholder="Describe a sound or musical idea…" /></label></div>
      <div className="header-status"><StatusPill good={snapshot !== null}>{snapshot === null ? "Bridge" : `${snapshot.activeVoices} voices`}</StatusPill><StatusPill good={snapshot?.architecture === "x86_64"}>x86_64</StatusPill><button onClick={() => void native.undo().then((value) => announce(String(value)))}>Undo</button><button onClick={() => void native.redo().then((value) => announce(String(value)))}>Redo</button><button className="panic" onClick={() => void native.panic().then((value) => announce(String(value)))}>Panic</button></div>
    </header>
    <nav className="navigation" aria-label="Primary">
      {tabs.map((name, index) => <button key={name} className={tab === name ? "active" : ""} aria-current={tab === name ? "page" : undefined} onClick={() => setTab(name)} onKeyDown={(event) => {
        if (event.key !== "ArrowLeft" && event.key !== "ArrowRight") return;
        const next = (index + (event.key === "ArrowRight" ? 1 : tabs.length - 1)) % tabs.length;
        setTab(tabs[next] ?? "SYNTH");
      }}>{name}</button>)}
    </nav>
    <main>
      {snapshot === null ? <section className="surface recovery"><h2>Native snapshot unavailable</h2><p>{error}</p><button onClick={() => void refresh()}>Retry complete recovery</button></section> : <>
        {tab === "SYNTH" && <SynthView snapshot={snapshot} visible={visible} preferences={preferences} announce={announce} refresh={refresh} publishSnapshot={setSnapshot} />}
        {tab === "COMPOSE" && <ComposeView initial={snapshot.composition} announce={announce}
          publishComposition={(composition) => setSnapshot((current) => current === null ? null : { ...current, composition })} />}
        {tab === "JARVIS" && <JarvisView draft={jarvisDraft} onDraftChange={setJarvisDraft}
          announce={announce} refreshSound={refresh}
          publishComposition={(composition) => setSnapshot((current) => current === null ? null : { ...current, composition })}
          reviewComposition={() => setTab("COMPOSE")} />}
        {tab === "FX" && <FxView snapshot={snapshot} announce={announce} />}
        {tab === "HISTORY" && <PersistenceView announce={announce} refreshSoundSnapshot={refresh} />}
        {tab === "SETTINGS" && <div className="settings-layout"><section className="surface settings"><div className="section-heading"><div><span>UI</span><h2>Performance + accessibility</h2></div><small>Local presentation only</small></div><label><input type="checkbox" checked={preferences.lowGraphics} onChange={(event) => setPreferences((value) => ({ ...value, lowGraphics: event.currentTarget.checked }))} />Low Graphics · 2D canvas at 12 FPS</label><label><input type="checkbox" checked={preferences.reducedMotion} onChange={(event) => setPreferences((value) => ({ ...value, reducedMotion: event.currentTarget.checked }))} />Reduced Motion · render only on state changes</label><button onClick={() => void refresh()}>Request complete native snapshot</button><p>No remote fonts, trackers, CDNs, providers, or runtime assets are loaded.</p></section><AssistantProviderSettings announce={announce} /></div>}
      </>}
    </main>
    <footer><span aria-live="polite">{announcement}</span><span>{snapshot?.version ?? "offline"} · {visible ? "visible" : "graphics paused"}</span></footer>
  </div>;
}
