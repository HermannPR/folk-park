import { useCallback, useEffect, useState } from "react";
import { getNativeFunction } from "@juce/index.js";
import { parseAssistantProviderStatus, type AssistantProviderStatus } from "./protocol.ts";

const getAssistantProviderStatus = getNativeFunction("getAssistantProviderStatus");

export function AssistantProviderSettings({ announce }:
  { announce: (message: string) => void }) {
  const [status, setStatus] = useState<AssistantProviderStatus | null>(null);
  const [error, setError] = useState("Checking native security boundary…");

  const refresh = useCallback(async () => {
    const value = await getAssistantProviderStatus();
    const parsed = parseAssistantProviderStatus(value);
    if (parsed === null) {
      const message = typeof value === "string" ? value.slice(0, 512)
        : "Rejected malformed provider status";
      setStatus(null); setError(message); announce(message); return;
    }
    setStatus(parsed); setError(""); announce(parsed.message);
  }, [announce]);

  useEffect(() => { void refresh(); }, [refresh]);

  return <section className="surface provider-settings">
    <div className="section-heading"><div><span>M7</span><h2>Jarvis privacy + providers</h2></div><small>Native security boundary</small></div>
    {status === null ? <div className="provider-error"><p>{error}</p><button onClick={() => void refresh()}>Retry security status</button></div> : <>
      <div className="provider-cards">
        <article className="active"><span>Current mode</span><strong>Offline deterministic</strong><p>Sound walkthroughs and composition text work locally with no account, API key, or network.</p></article>
        <article><span>Remote model</span><strong>Not selected</strong><p>No provider adapter or outbound request is enabled. Product-owner selection and per-request consent are still required.</p></article>
        <article className={status.keychainAvailable ? "ready" : ""}><span>Credential storage</span><strong>{status.keychainAvailable ? "macOS Keychain supported" : "Unavailable"}</strong><p>Native opaque-byte storage exists for a future opt-in provider. Credentials never enter React, presets, DAW state, logs, or Git.</p></article>
      </div>
      <div className="privacy-boundary" role="note"><strong>What can leave this Mac today?</strong><span>Nothing through Jarvis. The current provider path is disabled, and no credential is configured or requested.</span></div>
      <ul className="provider-rules"><li>Offline/manual operation remains complete.</li><li>Any future remote request needs a visible disclosure and one-request consent.</li><li>Provider output repeats native schema, UUID, catalog, size, and finiteness validation.</li><li>Only explicit producer acceptance can commit a proposal.</li></ul>
      <div className="actions"><button onClick={() => void refresh()}>Refresh native status</button><button disabled>Configure provider after selection</button></div>
      <p className="provider-message">{status.message}</p>
    </>}
  </section>;
}
