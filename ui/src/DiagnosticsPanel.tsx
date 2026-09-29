import { useState } from "react";
import { getNativeFunction } from "@juce/index.js";

const native = {
  preview: getNativeFunction("previewDiagnostics"),
  copy: getNativeFunction("copyPreviewedDiagnostics"),
};

type DiagnosticsPreview = { previewId: string; text: string };

function parsePreview(value: unknown): DiagnosticsPreview | null {
  if (typeof value !== "object" || value === null) return null;
  const record = value as Record<string, unknown>;
  if (Object.keys(record).length !== 2 || typeof record.previewId !== "string"
      || typeof record.text !== "string" || record.previewId.length === 0
      || record.previewId.length > 64 || record.text.length === 0
      || record.text.length >= 4096 || !record.text.startsWith("folk park diagnostics\nschema: 1\n"))
    return null;
  return { previewId: record.previewId, text: record.text };
}

export function DiagnosticsPanel({ announce }: { announce: (message: string) => void }) {
  const [preview, setPreview] = useState<DiagnosticsPreview | null>(null);
  const [status, setStatus] = useState("Nothing enters the clipboard until you preview and copy.");

  const createPreview = async () => {
    setPreview(null);
    const parsed = parsePreview(await native.preview());
    if (parsed === null) {
      setStatus("Native diagnostics preview was rejected as malformed.");
      return;
    }
    setPreview(parsed);
    setStatus("Review the complete bounded report before copying it.");
    announce("Fresh diagnostics preview created; clipboard unchanged");
  };

  const copyExactPreview = async () => {
    if (preview === null) return;
    const result = String(await native.copy(preview.previewId));
    setStatus(result);
    announce(result);
  };

  return <section className="surface diagnostics-panel">
    <div className="section-heading"><div><span>M8</span><h2>Privacy-safe diagnostics</h2></div><small>Maximum 4 KiB</small></div>
    <p>{status}</p>
    <ul className="provider-rules">
      <li>Includes build, host, audio setup, fixed subsystem codes, and fault counters.</li>
      <li>Excludes file paths, project or preset names, prompts, audio, and credentials.</li>
      <li>Previewing never writes a file, preference, project state, network request, or clipboard.</li>
    </ul>
    <div className="actions">
      <button className="primary" onClick={() => void createPreview()}>Preview diagnostics</button>
      <button disabled={preview === null} onClick={() => void copyExactPreview()}>Copy exact preview</button>
    </div>
    {preview !== null && <pre aria-label="Exact diagnostics clipboard preview">{preview.text}</pre>}
  </section>;
}
