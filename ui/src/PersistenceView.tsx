import { useCallback, useEffect, useState } from "react";
import { getNativeFunction } from "@juce/index.js";
import { parseHistoryComparison, parsePersistenceWorkspace,
  type HistoryComparison, type PersistenceWorkspace } from "./protocol.ts";

const native = {
  getWorkspace: getNativeFunction("getPersistenceWorkspace"),
  savePreset: getNativeFunction("savePreset"),
  loadPreset: getNativeFunction("loadPreset"),
  choosePresetFile: getNativeFunction("choosePresetFile"),
  relinkPresetAsset: getNativeFunction("relinkPresetAsset"),
  setPresetFavorite: getNativeFunction("setPresetFavorite"),
  recallHistory: getNativeFunction("recallHistory"),
  setHistoryFavorite: getNativeFunction("setHistoryFavorite"),
  setHistoryDeleted: getNativeFunction("setHistoryDeleted"),
  compareHistory: getNativeFunction("compareHistory"),
  setHistoryRetention: getNativeFunction("setHistoryRetention"),
  cleanupHistory: getNativeFunction("cleanupHistory"),
};

type Props = {
  announce: (message: string) => void;
  refreshSoundSnapshot: () => Promise<void>;
};

const readableDate = (timestamp: number) => new Date(timestamp).toLocaleString([], {
  year: "numeric", month: "short", day: "2-digit", hour: "2-digit", minute: "2-digit",
});

export function PersistenceView({ announce, refreshSoundSnapshot }: Props) {
  const [workspace, setWorkspace] = useState<PersistenceWorkspace | null>(null);
  const [search, setSearch] = useState("");
  const [favoritesOnly, setFavoritesOnly] = useState(false);
  const [includeDeleted, setIncludeDeleted] = useState(false);
  const [name, setName] = useState("New sound");
  const [author, setAuthor] = useState("");
  const [tags, setTags] = useState("");
  const [genre, setGenre] = useState("");
  const [emotion, setEmotion] = useState("");
  const [description, setDescription] = useState("");
  const [favorite, setFavorite] = useState(false);
  const [allowOverwrite, setAllowOverwrite] = useState(false);
  const [retention, setRetention] = useState(180);
  const [compareIds, setCompareIds] = useState<string[]>([]);
  const [comparison, setComparison] = useState<HistoryComparison | null>(null);

  const refresh = useCallback(async () => {
    const value = await native.getWorkspace(search.slice(0, 128), favoritesOnly, includeDeleted);
    const parsed = parsePersistenceWorkspace(value);
    if (parsed === null) { announce(typeof value === "string" ? value : "Rejected malformed persistence workspace"); return; }
    setWorkspace(parsed); setRetention(parsed.status.retentionDays);
  }, [announce, favoritesOnly, includeDeleted, search]);

  useEffect(() => { void refresh(); }, [refresh]);

  const refreshAfterSoundAction = async (operation: Promise<unknown>, success: string) => {
    const value = await operation;
    if (typeof value === "string") announce(value);
    else { announce(success); await refreshSoundSnapshot(); }
    await refresh();
  };
  const refreshAfterWorkspaceAction = async (operation: Promise<unknown>, success: string) => {
    const value = await operation;
    const parsed = parsePersistenceWorkspace(value);
    if (parsed === null) announce(typeof value === "string" ? value : "Rejected malformed persistence response");
    else { setWorkspace(parsed); setRetention(parsed.status.retentionDays); announce(success); }
  };
  const save = async () => {
    const parsedTags = [...new Set(tags.split(",").map((tag) => tag.trim()).filter(Boolean))];
    await refreshAfterWorkspaceAction(native.savePreset(name.trim(), author.trim(), parsedTags,
      genre.trim(), emotion.trim(), description.trim(), favorite, allowOverwrite),
    allowOverwrite ? "Preset replacement saved atomically" : "Preset saved atomically");
  };
  const toggleCompare = (id: string) => setCompareIds((ids) => ids.includes(id)
    ? ids.filter((value) => value !== id) : [...ids.slice(-1), id]);
  const compare = async () => {
    if (compareIds.length !== 2) { announce("Select exactly two history entries to compare"); return; }
    const value = await native.compareHistory(compareIds[0], compareIds[1]);
    const parsed = parseHistoryComparison(value);
    if (parsed === null) announce(typeof value === "string" ? value : "Rejected malformed comparison");
    else { setComparison(parsed); announce("Two history entries compared without changing the current sound"); }
  };

  if (workspace === null)
    return <section className="surface recovery"><h2>Opening local library…</h2><p>Preset and history work stays off the audio callback.</p><button onClick={() => void refresh()}>Retry</button></section>;

  const status = workspace.status;
  return <div className="history-layout">
    <section className="surface persistence-status wide-card">
      <div className="section-heading"><div><span>M6</span><h2>Native presets + recoverable history</h2></div><small>{status.historyAvailable ? "online" : "degraded"}</small></div>
      <p>{status.message}</p>
      <div className="persistence-badges"><span className={status.presetAvailable ? "ready" : "warning"}>Presets {status.presetAvailable ? "ready" : "unavailable"}</span><span className={status.historyAvailable ? "ready" : "warning"}>History {status.historyAvailable ? "ready" : "unavailable"}</span><span>Current · {status.currentPresetName}{status.currentPresetDirty ? " · unsaved changes" : ""}</span></div>
      {workspace.presetError && <p className="warning-text">Preset library: {workspace.presetError}</p>}
      {workspace.historyError && <p className="warning-text">History: {workspace.historyError}. Composition acceptance and audio remain available.</p>}
    </section>

    {status.missingAssets.length > 0 && <section className="surface missing-assets wide-card">
      <div className="section-heading"><div><span>RECOVER</span><h2>Missing preset assets</h2></div><small>Exact hash required</small></div>
      <p>The sound has not changed. Select only the original matching WAV; a different file is rejected.</p>
      {status.missingAssets.map((asset) => <div className="asset-row" key={asset.slot}><div><strong>{asset.slot === "oscillatorA" ? "Oscillator A" : "Oscillator B"}</strong><small>{asset.displayName} · {(asset.byteSize / 1024).toFixed(1)} KiB · {asset.sha256.slice(0, 12)}…</small></div><button onClick={() => void refreshAfterSoundAction(native.relinkPresetAsset(asset.slot === "oscillatorA" ? 0 : 1), "Matching asset relinked and preset applied")}>Select matching WAV</button></div>)}
    </section>}

    <section className="surface preset-save">
      <div className="section-heading"><div><span>SAVE</span><h2>Current sound</h2></div><small>Deterministic JSON</small></div>
      <div className="preset-form"><label>Name<input maxLength={96} value={name} onChange={(event) => setName(event.currentTarget.value)} /></label><label>Author<input maxLength={96} value={author} onChange={(event) => setAuthor(event.currentTarget.value)} /></label><label>Tags · comma separated<input maxLength={512} value={tags} onChange={(event) => setTags(event.currentTarget.value)} /></label><label>Genre<input maxLength={64} value={genre} onChange={(event) => setGenre(event.currentTarget.value)} /></label><label>Emotion<input maxLength={64} value={emotion} onChange={(event) => setEmotion(event.currentTarget.value)} /></label><label>Description<textarea maxLength={1024} value={description} onChange={(event) => setDescription(event.currentTarget.value)} /></label></div>
      <div className="check-row"><label><input type="checkbox" checked={favorite} onChange={(event) => setFavorite(event.currentTarget.checked)} />Favorite</label><label><input type="checkbox" checked={allowOverwrite} onChange={(event) => setAllowOverwrite(event.currentTarget.checked)} />Explicitly replace current preset</label></div>
      <div className="actions"><button className="primary" disabled={!status.presetAvailable || name.trim().length === 0} onClick={() => void save()}>Save native preset</button><button disabled={!status.presetAvailable} onClick={() => void refreshAfterSoundAction(native.choosePresetFile(), "External preset validated, localized, and applied")}>Import .folkparkpreset</button></div>
    </section>

    <section className="surface preset-browser">
      <div className="section-heading"><div><span>LIBRARY</span><h2>Preset browser</h2></div><small>{workspace.presets.length} local</small></div>
      <div className="library-list">{workspace.presets.length === 0 ? <p>No saved presets yet.</p> : workspace.presets.map((preset) => <article key={preset.id} className={preset.id === status.currentPresetId ? "current" : ""}><div><strong>{preset.name}</strong><small>{[preset.author, preset.genre, preset.emotion].filter(Boolean).join(" · ") || "Uncategorized"}</small><small>{preset.fileName}{preset.missingAssets ? " · asset missing" : ""}</small></div><div><button disabled={preset.missingAssets} onClick={() => void refreshAfterSoundAction(native.loadPreset(preset.id), `Loaded ${preset.name}`)}>Load</button><button aria-label={`${preset.favorite ? "Remove" : "Add"} ${preset.name} favorite`} onClick={() => void refreshAfterWorkspaceAction(native.setPresetFavorite(preset.id, !preset.favorite), "Preset favorite updated")}>{preset.favorite ? "★" : "☆"}</button></div></article>)}</div>
    </section>

    <section className="surface history-browser wide-card">
      <div className="section-heading"><div><span>HISTORY</span><h2>Accepted compositions</h2></div><small>{workspace.history.length} shown</small></div>
      <div className="history-tools"><label>Search<input maxLength={128} value={search} onChange={(event) => setSearch(event.currentTarget.value)} /></label><label><input type="checkbox" checked={favoritesOnly} onChange={(event) => setFavoritesOnly(event.currentTarget.checked)} />Favorites only</label><label><input type="checkbox" checked={includeDeleted} onChange={(event) => setIncludeDeleted(event.currentTarget.checked)} />Recoverable trash</label><button onClick={() => void refresh()}>Search</button><button disabled={compareIds.length !== 2} onClick={() => void compare()}>Compare selected</button></div>
      <div className="history-list">{workspace.history.length === 0 ? <p>No matching accepted compositions.</p> : workspace.history.map((entry) => <article key={entry.id} className={entry.deleted ? "deleted" : ""}><label className="compare-check"><input type="checkbox" checked={compareIds.includes(entry.id)} onChange={() => toggleCompare(entry.id)} />Compare</label><div><strong>{entry.tags.join(" · ") || "Accepted composition"}</strong><small>{readableDate(entry.createdUnixMs)} · {entry.generatorVersion}{entry.parentId ? " · variation" : ""}</small><small>{entry.presetId ? "Sound preset linked" : "Session sound"}{entry.deleted ? " · recoverable trash" : ""}</small></div><div><button disabled={entry.deleted} onClick={() => void refreshAfterSoundAction(native.recallHistory(entry.id), "History composition and linked sound recalled")}>Recall</button><button onClick={() => void refreshAfterWorkspaceAction(native.setHistoryFavorite(entry.id, !entry.favorite), "History favorite updated")}>{entry.favorite ? "★" : "☆"}</button><button onClick={() => void refreshAfterWorkspaceAction(native.setHistoryDeleted(entry.id, !entry.deleted), entry.deleted ? "History entry restored" : "History entry moved to recoverable trash")}>{entry.deleted ? "Restore" : "Trash"}</button></div></article>)}</div>
    </section>

    {comparison && <section className="surface comparison wide-card"><div className="section-heading"><div><span>COMPARE</span><h2>Intent difference</h2></div><small>No live state changed</small></div><div className="comparison-grid">{[comparison.first, comparison.second].map((item) => <article key={item.id}><strong>{item.genre} · {item.emotion}</strong><span>{item.key} {item.scale.replaceAll("_", " ")} · {item.tempoBpm.toFixed(1)} BPM · {item.bars} bars</span><span>Seed {item.seed} · {item.clipCount} clips · {item.noteCount} notes</span><small>{item.tags.join(" · ")}</small></article>)}</div></section>}

    <section className="surface retention wide-card">
      <div className="section-heading"><div><span>RETENTION</span><h2>Local recovery policy</h2></div><small>Favorites protected by default</small></div>
      <label>Keep history for <input type="number" min="1" max="3650" value={retention} onChange={(event) => setRetention(Number(event.currentTarget.value))} /> days</label>
      <div className="actions"><button disabled={!status.historyAvailable} onClick={() => void refreshAfterWorkspaceAction(native.setHistoryRetention(retention), "Retention preference saved")}>Save retention</button><button disabled={!status.historyAvailable} onClick={() => { if (window.confirm("Permanently remove expired non-favorite history entries now?")) void refreshAfterWorkspaceAction(native.cleanupHistory(true), "Expired non-favorite history cleaned"); }}>Clean expired non-favorites</button></div>
    </section>
  </div>;
}
