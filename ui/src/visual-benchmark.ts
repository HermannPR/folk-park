import { performance } from "node:perf_hooks";
import { calculateSpectrum, interpolateFrame } from "./visual-math.ts";
import { maximumWavetableFrames, wavetableSamplesPerFrame, type WavetableSnapshot } from "./protocol.ts";

const table: WavetableSnapshot = {
  frameCount: maximumWavetableFrames,
  samplesPerFrame: wavetableSamplesPerFrame,
  samples: Array.from({ length: maximumWavetableFrames * wavetableSamplesPerFrame }, (_, index) =>
    Math.sin(index * 0.071) * 0.72 + Math.sin(index * 0.193) * 0.28),
};
const iterations = 10_000;
let checksum = 0;
const started = performance.now();
for (let iteration = 0; iteration < iterations; ++iteration) {
  const frame = interpolateFrame(table, (iteration % 1000) / 999);
  const spectrum = calculateSpectrum(frame);
  checksum += frame[iteration % frame.length] ?? 0;
  checksum += spectrum[iteration % spectrum.length] ?? 0;
}
const elapsedMs = performance.now() - started;
if (!Number.isFinite(checksum) || !Number.isFinite(elapsedMs))
  throw new Error("visual benchmark produced a non-finite result");
console.log(JSON.stringify({ architecture: process.arch, frames: table.frameCount,
  samplesPerFrame: table.samplesPerFrame, iterations, elapsedMs,
  microsecondsPerAnalysis: elapsedMs * 1000 / iterations, checksum }));
