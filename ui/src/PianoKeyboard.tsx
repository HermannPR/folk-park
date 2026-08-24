import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { getNativeFunction } from "@juce/index.js";

const noteOn = getNativeFunction("previewNoteOn");
const noteOff = getNativeFunction("previewNoteOff");
const releaseAll = getNativeFunction("releasePreviewNotes");

const firstNote = 36;
const lastNote = 83;
const blackPitchClasses = new Set([1, 3, 6, 8, 10]);
const computerKeys = ["a", "w", "s", "e", "d", "f", "t", "g", "y", "h", "u", "j", "k", "o", "l", "p"];
const noteNames = ["C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B"];

function nameFor(note: number) {
  return `${noteNames[note % 12]}${Math.floor(note / 12) - 1}`;
}

function isTypingTarget(target: EventTarget | null) {
  return target instanceof HTMLInputElement || target instanceof HTMLSelectElement
    || target instanceof HTMLTextAreaElement
    || (target instanceof HTMLElement && target.isContentEditable);
}

export function PianoKeyboard({ announce, compact = false }:
  { announce: (message: string) => void; compact?: boolean }) {
  const sources = useRef(new Map<number, Set<string>>());
  const [active, setActive] = useState<Set<number>>(() => new Set());
  const [computerBase, setComputerBase] = useState(48);
  const keyToNote = useMemo(() => new Map(computerKeys.map((key, index) =>
    [key, computerBase + index])), [computerBase]);

  const start = useCallback((note: number, source: string) => {
    const noteSources = sources.current.get(note) ?? new Set<string>();
    if (noteSources.has(source)) return;
    const wasInactive = noteSources.size === 0;
    noteSources.add(source);
    sources.current.set(note, noteSources);
    setActive(new Set(sources.current.keys()));
    if (wasInactive)
      void noteOn(note, 104).then((result) => { if (result !== true) announce(String(result)); });
  }, [announce]);

  const stop = useCallback((note: number, source: string) => {
    const noteSources = sources.current.get(note);
    if (noteSources === undefined || !noteSources.delete(source)) return;
    if (noteSources.size === 0) {
      sources.current.delete(note);
      void noteOff(note).then((result) => { if (result !== true) announce(String(result)); });
    }
    setActive(new Set(sources.current.keys()));
  }, [announce]);

  const stopEverything = useCallback(() => {
    if (sources.current.size > 0) {
      sources.current.clear();
      setActive(new Set());
    }
    void releaseAll();
  }, []);

  useEffect(() => {
    const down = (event: KeyboardEvent) => {
      if (event.repeat || event.metaKey || event.ctrlKey || event.altKey || isTypingTarget(event.target)) return;
      const note = keyToNote.get(event.key.toLowerCase());
      if (note === undefined) return;
      event.preventDefault();
      event.stopPropagation();
      start(note, `key:${event.code}`);
    };
    const up = (event: KeyboardEvent) => {
      const note = keyToNote.get(event.key.toLowerCase());
      if (note === undefined) return;
      event.preventDefault();
      event.stopPropagation();
      stop(note, `key:${event.code}`);
    };
    const visibility = () => { if (document.visibilityState !== "visible") stopEverything(); };
    window.addEventListener("keydown", down);
    window.addEventListener("keyup", up);
    window.addEventListener("blur", stopEverything);
    document.addEventListener("visibilitychange", visibility);
    return () => {
      window.removeEventListener("keydown", down);
      window.removeEventListener("keyup", up);
      window.removeEventListener("blur", stopEverything);
      document.removeEventListener("visibilitychange", visibility);
      sources.current.clear();
      void releaseAll();
    };
  }, [start, stop, stopEverything]);

  const notes = Array.from({ length: lastNote - firstNote + 1 }, (_, index) => firstNote + index);
  return <section className={`surface wide-card keyboard-card ${compact ? "compact" : ""}`}>
    <div className="section-heading"><div><span>♫</span><h2>{compact ? "Always-on audition" : "Four-octave audition keyboard"}</h2></div><div className="keyboard-range"><small>C2–B5 · A–P zone {nameFor(computerBase)}</small><button aria-label="Shift computer keyboard down one octave" disabled={computerBase <= 36} onClick={() => { stopEverything(); setComputerBase((value) => Math.max(36, value - 12)); }}>Oct −</button><button aria-label="Shift computer keyboard up one octave" disabled={computerBase >= 60} onClick={() => { stopEverything(); setComputerBase((value) => Math.min(60, value + 12)); }}>Oct +</button></div></div>
    <div className="piano-keyboard" role="group" aria-label="Four-octave playable piano keyboard from C2 to B5">
      {notes.map((note) => {
        const black = blackPitchClasses.has(note % 12);
        const computerKeyIndex = note - computerBase;
        const computerKey = computerKeyIndex >= 0 && computerKeyIndex < computerKeys.length
          ? computerKeys[computerKeyIndex] : undefined;
        return <button key={note} type="button" className={`piano-key ${black ? "black" : "white"} ${active.has(note) ? "pressed" : ""}`}
          aria-label={`Play ${nameFor(note)}${computerKey ? `, computer key ${computerKey.toUpperCase()}` : ""}`}
          onPointerDown={(event) => { event.preventDefault(); event.currentTarget.setPointerCapture(event.pointerId); start(note, `pointer:${event.pointerId}`); }}
          onPointerUp={(event) => { stop(note, `pointer:${event.pointerId}`); event.currentTarget.blur(); }}
          onPointerCancel={(event) => stop(note, `pointer:${event.pointerId}`)}
          onKeyDown={(event) => { if (!event.repeat && (event.key === "Enter" || event.key === " ")) { event.preventDefault(); start(note, `access:${note}`); } }}
          onKeyUp={(event) => { if (event.key === "Enter" || event.key === " ") stop(note, `access:${note}`); }}>
          <span>{nameFor(note)}</span>{computerKey && <kbd>{computerKey.toUpperCase()}</kbd>}
        </button>;
      })}
    </div>
    <p>Touch or click all four octaves. A–P mapped computer keys follow the selected octave zone. Holding a key sustains once without macOS repeat retriggering; notes release automatically if focus or touch is lost.</p>
  </section>;
}
