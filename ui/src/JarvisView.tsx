import { useCallback, useEffect, useMemo, useState } from "react";
import { getNativeFunction } from "@juce/index.js";
import { parseGuidedProgress, parseJarvisAuditionState, parseJarvisCompositionResult,
  type CompositionSnapshot, type GuidedProgress, type GuidedQuestion, type JarvisAuditionState,
  type JarvisCompositionResult } from "./protocol.ts";

const native = {
  getJarvisState: getNativeFunction("getJarvisState"),
  getJarvisQuestions: getNativeFunction("getJarvisQuestions"),
  createJarvisSoundProposal: getNativeFunction("createJarvisSoundProposal"),
  auditionJarvisSide: getNativeFunction("auditionJarvisSide"),
  acceptJarvisProposal: getNativeFunction("acceptJarvisProposal"),
  rejectJarvisProposal: getNativeFunction("rejectJarvisProposal"),
  createJarvisComposition: getNativeFunction("createJarvisComposition"),
};

type SoundAnswers = {
  musicalRole: string;
  timbre: string;
  articulation: string;
  movement: string;
  space: string;
  intensity: number | null;
  genreContext: string;
  referenceDescription: string;
};

type Message = { id: number; speaker: "jarvis" | "producer"; title: string; body: string };

const emptyAnswers: SoundAnswers = { musicalRole: "", timbre: "", articulation: "",
  movement: "", space: "", intensity: null, genreContext: "",
  referenceDescription: "" };

const questionLabels: Record<keyof SoundAnswers, string> = {
  musicalRole: "Musical role", timbre: "Timbre", articulation: "Articulation",
  movement: "Movement", space: "Space", intensity: "Intensity",
  genreContext: "Genre context", referenceDescription: "Reference description",
};

const answerKeyByQuestionId = {
  "musical-role": "musicalRole",
  timbre: "timbre",
  articulation: "articulation",
  movement: "movement",
  space: "space",
  intensity: "intensity",
  "genre-context": "genreContext",
  "reference-description": "referenceDescription",
} as const satisfies Record<GuidedQuestion["id"], keyof SoundAnswers>;

function boundedError(value: unknown, fallback: string) {
  return typeof value === "string" && value.trim().length > 0
    ? value.slice(0, 512) : fallback;
}

export function JarvisView({ draft, onDraftChange, announce, refreshSound,
  publishComposition, reviewComposition }: {
  draft: string;
  onDraftChange: (value: string) => void;
  announce: (message: string) => void;
  refreshSound: () => Promise<void>;
  publishComposition: (composition: CompositionSnapshot) => void;
  reviewComposition: () => void;
}) {
  const [target, setTarget] = useState<"sound" | "composition">("sound");
  const [entryMode, setEntryMode] = useState<"describe" | "guided">("describe");
  const [seed, setSeed] = useState(20260823);
  const [answers, setAnswers] = useState<SoundAnswers>(emptyAnswers);
  const [progress, setProgress] = useState<GuidedProgress | null>(null);
  const [audition, setAudition] = useState<JarvisAuditionState | null>(null);
  const [composition, setComposition] = useState<JarvisCompositionResult | null>(null);
  const [busy, setBusy] = useState(false);
  const [messages, setMessages] = useState<Message[]>([{
    id: 1, speaker: "jarvis", title: "Ready offline",
    body: "Describe a sound or musical idea. I will explain what I understood and create only a reviewable proposal.",
  }]);

  const addMessage = useCallback((speaker: Message["speaker"], title: string, body: string) => {
    setMessages((current) => [...current.slice(-11), {
      id: (current.at(-1)?.id ?? 0) + 1, speaker, title: title.slice(0, 96), body: body.slice(0, 1024),
    }]);
  }, []);

  const soundPayload = useCallback(() => {
    const prompt = draft.trim() || `Guided sound for ${answers.musicalRole || "the selected role"}`;
    return { entryMode, seed, prompt, answers: {
      musicalRole: answers.musicalRole, timbre: answers.timbre,
      articulation: answers.articulation, movement: answers.movement,
      space: answers.space, genreContext: answers.genreContext,
      referenceDescription: entryMode === "describe"
        ? (answers.referenceDescription || prompt) : answers.referenceDescription,
      ...(answers.intensity === null ? {} : { intensity: answers.intensity }),
    } };
  }, [answers, draft, entryMode, seed]);

  useEffect(() => {
    let mounted = true;
    void native.getJarvisState().then((value) => {
      if (!mounted) return;
      const parsed = parseJarvisAuditionState(value);
      if (parsed !== null) setAudition(parsed);
    });
    return () => { mounted = false; };
  }, []);

  const askQuestions = useCallback(async () => {
    setBusy(true);
    const result = await native.getJarvisQuestions(soundPayload());
    const parsed = parseGuidedProgress(result);
    setBusy(false);
    if (parsed === null) {
      const message = boundedError(result, "Jarvis returned an invalid guided step");
      addMessage("jarvis", "Could not continue", message); announce(message); return;
    }
    setProgress(parsed);
    if (parsed.readyForProposal) {
      addMessage("jarvis", "Intent complete",
        "I have enough bounded context to build a parameter proposal. Nothing changes until you audition proposal B.");
    } else {
      addMessage("jarvis", "Two focused questions",
        `${Math.round(parsed.completion * 100)}% complete · ${parsed.questions.map((question) => question.prompt).join(" ")}`);
    }
  }, [addMessage, announce, soundPayload]);

  const submitSound = async () => {
    if (audition?.active) {
      addMessage("jarvis", "Finish the current review", "Accept or reject the active A/B proposal first.");
      return;
    }
    if (entryMode === "describe" && draft.trim().length === 0) {
      addMessage("jarvis", "Description needed", "Tell me the role, tone, movement, or reference you want.");
      return;
    }
    addMessage("producer", entryMode === "guided" ? "Guided sound request" : "Sound request",
      draft.trim() || "Use my guided answers");
    if (entryMode === "guided" && !progress?.readyForProposal) { await askQuestions(); return; }
    setBusy(true);
    const result = await native.createJarvisSoundProposal(soundPayload());
    const parsed = parseJarvisAuditionState(result);
    setBusy(false);
    if (parsed === null || parsed.proposal === null) {
      const message = boundedError(result, "Jarvis returned an invalid sound proposal");
      addMessage("jarvis", "Proposal unavailable", message); announce(message); return;
    }
    setAudition(parsed);
    addMessage("jarvis", "Proposal ready for A/B",
      parsed.summary || `${parsed.proposal.changes.length} parameter changes are ready to audition.`);
    announce("Jarvis proposal ready; original A remains audible");
  };

  const submitComposition = async () => {
    if (draft.trim().length === 0) {
      addMessage("jarvis", "Musical idea needed",
        "Try: four-bar dark house chords and melody in D minor at 124 BPM.");
      return;
    }
    addMessage("producer", "Composition request", draft.trim());
    setBusy(true);
    const result = await native.createJarvisComposition({ prompt: draft.trim(), seed });
    const parsed = parseJarvisCompositionResult(result);
    setBusy(false);
    if (parsed === null) {
      const message = boundedError(result, "Jarvis returned an invalid composition candidate");
      addMessage("jarvis", "Candidate unavailable", message); announce(message); return;
    }
    setComposition(parsed); publishComposition(parsed.composition);
    addMessage("jarvis", "Composition candidate created", parsed.summary);
    announce("Jarvis composition candidate is awaiting review and acceptance");
  };

  const changeAudition = async (action: "original" | "proposal" | "accept" | "reject") => {
    setBusy(true);
    const result = action === "accept" ? await native.acceptJarvisProposal()
      : action === "reject" ? await native.rejectJarvisProposal()
      : await native.auditionJarvisSide(action);
    const parsed = parseJarvisAuditionState(result);
    setBusy(false);
    if (parsed === null) {
      const message = boundedError(result, "Jarvis returned an invalid A/B state");
      addMessage("jarvis", "A/B action unavailable", message); announce(message); return;
    }
    setAudition(parsed); await refreshSound();
    const title = action === "accept" ? "Proposal accepted"
      : action === "reject" ? "Original restored"
      : action === "original" ? "Auditioning original A" : "Auditioning proposal B";
    addMessage("jarvis", title, parsed.message); announce(parsed.message);
  };

  const questionInputs = useMemo(() => progress?.questions ?? [], [progress]);
  const setAnswer = (key: keyof SoundAnswers, value: string | number | null) => {
    setAnswers((current) => ({ ...current, [key]: value }));
  };

  return <div className="jarvis-layout">
    <section className="surface jarvis-conversation">
      <div className="section-heading"><div><span>AI</span><h2>Jarvis production assistant</h2></div><small>Offline · deterministic · no account</small></div>
      <div className="jarvis-trust" role="note"><strong>Producer stays in control.</strong><span>No API key, network request, hidden edit, or automatic acceptance. This M7 assistant uses bounded local rules—not a general-purpose LLM.</span></div>
      <div className="jarvis-messages" aria-label="Jarvis conversation">
        {messages.map((message) => <article key={message.id} className={message.speaker}>
          <small>{message.speaker === "jarvis" ? "JARVIS" : "YOU"}</small>
          <strong>{message.title}</strong><p>{message.body}</p>
        </article>)}
      </div>
      <div className="jarvis-mode" role="group" aria-label="Assistant target">
        <button className={target === "sound" ? "active" : ""} aria-pressed={target === "sound"} onClick={() => setTarget("sound")}>Design a sound</button>
        <button className={target === "composition" ? "active" : ""} aria-pressed={target === "composition"} onClick={() => setTarget("composition")}>Compose MIDI</button>
      </div>
      {target === "sound" && <div className="jarvis-entry-mode" role="group" aria-label="Sound helper mode">
        <label><input type="radio" name="entry-mode" checked={entryMode === "describe"} onChange={() => { setEntryMode("describe"); setProgress(null); }} />Describe it</label>
        <label><input type="radio" name="entry-mode" checked={entryMode === "guided"} onChange={() => { setEntryMode("guided"); setProgress(null); }} />Walk me through it</label>
      </div>}
      <label className="jarvis-prompt">{target === "sound" ? "What should the sound do?" : "What should we compose?"}
        <textarea maxLength={1024} rows={3} value={draft} onChange={(event) => onDraftChange(event.currentTarget.value)} placeholder={target === "sound" ? "A warm, wide lead with slow movement and a short bright attack…" : "Four-bar dark house chords and melody in D minor at 124 BPM…"} />
        <span>{draft.length}/1024</span>
      </label>
      <div className="jarvis-submit-row"><label>Deterministic seed<input type="number" min="0" max="4294967295" value={seed} onChange={(event) => setSeed(Math.max(0, Math.min(4294967295, Number(event.currentTarget.value) || 0)))} /></label><button className="primary" disabled={busy} onClick={() => void (target === "sound" ? submitSound() : submitComposition())}>{busy ? "Working locally…" : target === "sound" ? (entryMode === "guided" && !progress?.readyForProposal ? "Start / continue walkthrough" : "Create reviewable proposal") : "Create composition candidate"}</button></div>
    </section>

    <aside className="jarvis-review">
      {target === "sound" && entryMode === "guided" && <section className="surface jarvis-questions">
        <div className="section-heading"><div><span>01</span><h2>Sound walkthrough</h2></div><small>{Math.round((progress?.completion ?? 0) * 100)}%</small></div>
        {questionInputs.length === 0 && <p>Jarvis asks no more than two focused questions at a time. Start the walkthrough to define role, tone, articulation, movement, space, intensity, and genre.</p>}
        {questionInputs.map((question) => {
          const key = answerKeyByQuestionId[question.id];
          return <label className="jarvis-question" key={question.id}><strong>{question.prompt}</strong><small>{question.purpose}</small>
            {key === "intensity" ? <><input type="range" min="0" max="1" step="0.01" value={answers.intensity ?? .5} onChange={(event) => setAnswer(key, Number(event.currentTarget.value))} /><output>{(answers.intensity ?? .5).toFixed(2)}</output></>
              : <input maxLength={key === "timbre" ? 256 : 128} value={String(answers[key] ?? "")} onChange={(event) => setAnswer(key, event.currentTarget.value)} placeholder={questionLabels[key]} />}
          </label>;
        })}
        {progress !== null && <div className="progress-track" aria-label={`${Math.round(progress.completion * 100)}% complete`}><i style={{ width: `${progress.completion * 100}%` }} /></div>}
        {questionInputs.length > 0 && <button disabled={busy || questionInputs.some((question) => {
          const value = answers[answerKeyByQuestionId[question.id]];
          return value === null || (typeof value === "string" && value.trim().length === 0);
        })} onClick={() => void askQuestions()}>Save answers and continue</button>}
      </section>}

      {target === "sound" && <section className="surface jarvis-proposal">
        <div className="section-heading"><div><span>AB</span><h2>Sound proposal</h2></div><small>{audition?.status ?? "idle"}</small></div>
        {audition?.proposal === null || audition === null ? <p>No active proposal. Your current sound remains untouched.</p> : <>
          <h3>{audition.proposal.explanation}</h3>
          <p>{audition.summary || audition.message}</p>
          <div className="proposal-metrics"><span>{audition.proposal.changes.length} changes</span><span>{Math.round(audition.proposal.confidence * 100)}% confidence</span><span>Explicit acceptance</span></div>
          {audition.proposal.assumptions.length > 0 && <details><summary>Assumptions</summary><ul>{audition.proposal.assumptions.map((assumption) => <li key={assumption}>{assumption}</li>)}</ul></details>}
          <div className="proposal-changes">{audition.proposal.changes.map((change) => <div key={change.parameterId}><strong>{change.parameterId}</strong><code>{change.currentNormalized.toFixed(2)} → {change.proposedNormalized.toFixed(2)}</code><small>{change.reason}</small></div>)}</div>
          {audition.active ? <><div className="ab-switch" role="group" aria-label="Original and proposal audition"><button className={audition.audibleSide === "original" ? "active" : ""} disabled={busy} onClick={() => void changeAudition("original")}>A · Original</button><button className={audition.audibleSide === "proposal" ? "active" : ""} disabled={busy} onClick={() => void changeAudition("proposal")}>B · Proposal</button></div><div className="actions"><button className="accept" disabled={busy} onClick={() => void changeAudition("accept")}>Accept B</button><button disabled={busy} onClick={() => void changeAudition("reject")}>Reject and restore A</button></div></> : <p className="proposal-finished">{audition.message}</p>}
        </>}
      </section>}

      {target === "composition" && <section className="surface jarvis-composition-review">
        <div className="section-heading"><div><span>02</span><h2>Composition candidate</h2></div><small>Acceptance separate</small></div>
        {composition === null ? <p>Jarvis extracts musical intent locally, then uses the deterministic M3 engine. A result is a candidate—not delivered MIDI.</p> : <><h3>{composition.summary}</h3><dl><div><dt>Key</dt><dd>{composition.intent.key} {composition.intent.scale.replaceAll("_", " ")}</dd></div><div><dt>Tempo</dt><dd>{composition.intent.tempoBpm} BPM</dd></div><div><dt>Length</dt><dd>{composition.intent.bars} bars</dd></div><div><dt>Parts</dt><dd>{composition.intent.parts.join(", ")}</dd></div><div><dt>Notes</dt><dd>{composition.composition.candidateNotes}</dd></div><div><dt>Seed</dt><dd>{composition.intent.seed}</dd></div></dl><button className="primary" onClick={reviewComposition}>Review piano roll before accepting</button></>}
      </section>}
    </aside>
  </div>;
}
