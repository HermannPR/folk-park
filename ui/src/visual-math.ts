import type { WavetableSnapshot } from "./protocol.ts";

export function interpolateFrame(table: WavetableSnapshot, position: number): number[] {
  const scaled = Math.max(0, Math.min(1, position)) * Math.max(0, table.frameCount - 1);
  const first = Math.floor(scaled);
  const second = Math.min(table.frameCount - 1, first + 1);
  const mix = scaled - first;
  return Array.from({ length: table.samplesPerFrame }, (_, sample) => {
    const left = table.samples[first * table.samplesPerFrame + sample] ?? 0;
    const right = table.samples[second * table.samplesPerFrame + sample] ?? 0;
    return left + (right - left) * mix;
  });
}

export function calculateSpectrum(samples: number[]): number[] {
  const bins = 24;
  return Array.from({ length: bins }, (_, bin) => {
    let real = 0;
    let imaginary = 0;
    for (let sample = 0; sample < samples.length; ++sample) {
      const phase = 2 * Math.PI * (bin + 1) * sample / samples.length;
      real += (samples[sample] ?? 0) * Math.cos(phase);
      imaginary -= (samples[sample] ?? 0) * Math.sin(phase);
    }
    return Math.min(1, Math.hypot(real, imaginary) / (samples.length * 0.35));
  });
}
