import { useEffect, useState } from "react";
import { getNativeFunction } from "@juce/index.js";
import { parseUiSnapshot, type ModulationRouteSnapshot, type UiSnapshot } from "./protocol.ts";

const setRoutes = getNativeFunction("setModulationRoutes");
const sources = ["Filter envelope", "Aux envelope", "LFO 1", "LFO 2", "LFO 3", "LFO 4",
  "Velocity", "Note", "Mod wheel", "Channel pressure"];
const destinations = ["Osc A position", "Osc B position", "Osc A pitch", "Osc B pitch",
  "Osc A level", "Osc B level", "Filter cutoff", "Filter resonance", "Filter drive",
  "Amplitude", "Pan", "Sub level", "Noise level"];
const curves = ["Linear", "Exponential", "S-curve"];
const defaultRoute: ModulationRouteSnapshot = { source: 2, destination: 0, amount: 0.35,
  curve: 0, enabled: true };

export function ModulationPanel({ initial, onSnapshot, announce }:
  { initial: ModulationRouteSnapshot[]; onSnapshot: (snapshot: UiSnapshot) => void;
    announce: (message: string) => void }) {
  const [routes, setLocalRoutes] = useState(() => initial.map((route) => ({ ...route })));
  const [dirty, setDirty] = useState(false);
  useEffect(() => {
    if (!dirty) setLocalRoutes(initial.map((route) => ({ ...route })));
  }, [dirty, initial]);
  const update = (index: number, patch: Partial<ModulationRouteSnapshot>) => {
    setLocalRoutes((current) => current.map((route, routeIndex) => routeIndex === index
      ? { ...route, ...patch } : route));
    setDirty(true);
  };
  const apply = async () => {
    const result = await setRoutes(routes);
    const snapshot = parseUiSnapshot(result);
    if (snapshot === null) { announce(String(result)); return; }
    onSnapshot(snapshot);
    setDirty(false);
    announce(`${snapshot.modulationRouteCount} modulation route${snapshot.modulationRouteCount === 1 ? "" : "s"} published safely`);
  };
  return <section className="surface wide-card modulation-panel">
    <div className="section-heading"><div><span>06</span><h2>Modulation matrix</h2></div><small>{routes.length}/32 routes · {dirty ? "review changes" : "native state"}</small></div>
    <div className="route-list">
      {routes.length === 0 && <p>No routes yet. Add one to connect an envelope, LFO, or performance source.</p>}
      {routes.map((route, index) => <div className="route-row" key={index}>
        <label>Source<select aria-label={`Route ${index + 1} source`} value={route.source} onChange={(event) => update(index, { source: Number(event.currentTarget.value) })}>{sources.map((name, source) => <option key={name} value={source}>{name}</option>)}</select></label>
        <label>Destination<select aria-label={`Route ${index + 1} destination`} value={route.destination} onChange={(event) => update(index, { destination: Number(event.currentTarget.value) })}>{destinations.map((name, destination) => <option key={name} value={destination}>{name}</option>)}</select></label>
        <label>Amount <output>{route.amount.toFixed(2)}</output><input aria-label={`Route ${index + 1} bipolar amount`} type="range" min="-1" max="1" step="0.01" value={route.amount} onChange={(event) => update(index, { amount: Number(event.currentTarget.value) })} /></label>
        <label>Curve<select aria-label={`Route ${index + 1} curve`} value={route.curve} onChange={(event) => update(index, { curve: Number(event.currentTarget.value) })}>{curves.map((name, curve) => <option key={name} value={curve}>{name}</option>)}</select></label>
        <label className="route-enabled"><input type="checkbox" checked={route.enabled} onChange={(event) => update(index, { enabled: event.currentTarget.checked })} />Enabled</label>
        <button aria-label={`Remove modulation route ${index + 1}`} onClick={() => { setLocalRoutes((current) => current.filter((_, routeIndex) => routeIndex !== index)); setDirty(true); }}>Remove</button>
      </div>)}
    </div>
    <div className="actions"><button disabled={routes.length >= 32} onClick={() => { setLocalRoutes((current) => [...current, { ...defaultRoute }]); setDirty(true); }}>Add route</button><button className="primary" disabled={!dirty} onClick={() => void apply()}>Apply reviewed matrix</button><button disabled={!dirty} onClick={() => { setLocalRoutes(initial.map((route) => ({ ...route }))); setDirty(false); }}>Discard changes</button></div>
  </section>;
}
