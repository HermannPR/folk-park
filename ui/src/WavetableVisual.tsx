import { useEffect, useMemo, useRef } from "react";
import * as THREE from "three";
import { chooseAnimationPolicy, type WavetableSnapshot } from "./protocol.ts";
import { calculateSpectrum, interpolateFrame } from "./visual-math.ts";

type Props = {
  name: "A" | "B";
  table: WavetableSnapshot;
  position: number;
  routeActive: boolean;
  visible: boolean;
  lowGraphics: boolean;
  reducedMotion: boolean;
};

function drawCanvas2d(canvas: HTMLCanvasElement, table: WavetableSnapshot, position: number) {
  const context = canvas.getContext("2d");
  if (context === null) return;
  const width = Math.max(1, canvas.clientWidth);
  const height = Math.max(1, canvas.clientHeight);
  const scale = Math.min(2, window.devicePixelRatio || 1);
  canvas.width = Math.round(width * scale);
  canvas.height = Math.round(height * scale);
  context.scale(scale, scale);
  context.clearRect(0, 0, width, height);
  const gradient = context.createLinearGradient(0, 0, width, height);
  gradient.addColorStop(0, "rgba(155, 135, 255, .34)");
  gradient.addColorStop(1, "rgba(50, 230, 210, .08)");
  context.fillStyle = gradient;
  context.fillRect(0, 0, width, height);
  const samples = interpolateFrame(table, position);
  context.beginPath();
  for (let index = 0; index < samples.length; ++index) {
    const x = index / Math.max(1, samples.length - 1) * width;
    const y = height * 0.5 - (samples[index] ?? 0) * height * 0.34;
    if (index === 0) context.moveTo(x, y); else context.lineTo(x, y);
  }
  context.strokeStyle = "#cfc7ff";
  context.lineWidth = 2;
  context.shadowColor = "#9b87ff";
  context.shadowBlur = 12;
  context.stroke();
}

export function WavetableVisual({ name, table, position, routeActive, visible,
  lowGraphics, reducedMotion }: Props) {
  const canvas = useRef<HTMLCanvasElement>(null);
  const currentFrame = useMemo(() => interpolateFrame(table, position), [table, position]);
  const spectrum = useMemo(() => calculateSpectrum(currentFrame), [currentFrame]);
  const policy = chooseAnimationPolicy({ visible, lowGraphics, reducedMotion });

  useEffect(() => {
    const element = canvas.current;
    if (element === null || policy.mode === "paused") return;
    if (policy.mode === "canvas2d" || policy.mode === "static") {
      drawCanvas2d(element, table, position);
      const observer = new ResizeObserver(() => drawCanvas2d(element, table, position));
      observer.observe(element);
      return () => observer.disconnect();
    }

    let renderer: THREE.WebGLRenderer;
    try {
      renderer = new THREE.WebGLRenderer({ canvas: element, alpha: true, antialias: true,
                                           powerPreference: "low-power" });
    } catch {
      drawCanvas2d(element, table, position);
      return;
    }
    renderer.setPixelRatio(Math.min(1.5, window.devicePixelRatio || 1));
    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(38, 1, 0.1, 100);
    camera.position.set(0, 2.8, 7.8);
    camera.lookAt(0, 0, 0);
    const group = new THREE.Group();
    scene.add(group);
    const geometries: THREE.BufferGeometry[] = [];
    const materials: THREE.LineBasicMaterial[] = [];
    for (let frame = 0; frame < table.frameCount; ++frame) {
      const points: THREE.Vector3[] = [];
      for (let sample = 0; sample < table.samplesPerFrame; ++sample) {
        const value = table.samples[frame * table.samplesPerFrame + sample] ?? 0;
        points.push(new THREE.Vector3(sample / (table.samplesPerFrame - 1) * 5 - 2.5,
          value * 1.15, frame / Math.max(1, table.frameCount - 1) * 2.4 - 1.2));
      }
      const geometry = new THREE.BufferGeometry().setFromPoints(points);
      const material = new THREE.LineBasicMaterial({ color: new THREE.Color().setHSL(
        0.69 - frame / Math.max(1, table.frameCount - 1) * 0.18, 0.72, 0.69),
        transparent: true, opacity: 0.34 });
      geometries.push(geometry);
      materials.push(material);
      group.add(new THREE.Line(geometry, material));
    }
    const markerGeometry = new THREE.SphereGeometry(0.09, 12, 8);
    const markerMaterial = new THREE.MeshBasicMaterial({ color: 0x6af5d0 });
    const marker = new THREE.Mesh(markerGeometry, markerMaterial);
    group.add(marker);

    const resize = () => {
      const width = Math.max(1, element.clientWidth);
      const height = Math.max(1, element.clientHeight);
      renderer.setSize(width, height, false);
      camera.aspect = width / height;
      camera.updateProjectionMatrix();
    };
    resize();
    const observer = new ResizeObserver(resize);
    observer.observe(element);
    let animationFrame = 0;
    let previous = 0;
    const interval = 1000 / policy.framesPerSecond;
    const render = (time: number) => {
      animationFrame = requestAnimationFrame(render);
      if (document.visibilityState !== "visible" || time - previous < interval) return;
      previous = time;
      const animatedPosition = Math.max(0, Math.min(1, position
        + (routeActive ? Math.sin(time * 0.0018) * 0.035 : 0)));
      marker.position.set(0, 0, animatedPosition * 2.4 - 1.2);
      group.rotation.y = -0.12 + Math.sin(time * 0.00022) * 0.04;
      renderer.render(scene, camera);
    };
    animationFrame = requestAnimationFrame(render);
    return () => {
      cancelAnimationFrame(animationFrame);
      observer.disconnect();
      geometries.forEach((geometry) => geometry.dispose());
      materials.forEach((material) => material.dispose());
      markerGeometry.dispose();
      markerMaterial.dispose();
      renderer.dispose();
    };
  }, [policy.mode, policy.framesPerSecond, position, routeActive, table]);

  return <section className="visual-card" aria-label={`Oscillator ${name} wavetable and spectrum`}>
    <div className="visual-heading"><span>OSC {name}</span><span>{table.frameCount} frames · {policy.mode}</span></div>
    <canvas ref={canvas} className="wavetable-canvas" />
    <div className="spectrum" aria-label={`Oscillator ${name} bounded spectrum view`}>
      {spectrum.map((magnitude, index) => <i key={index} style={{ height: `${Math.max(3, magnitude * 100)}%` }} />)}
    </div>
    <div className="visual-position"><span style={{ width: `${position * 100}%` }} /></div>
  </section>;
}
