import { resolve } from "node:path";
import { defineConfig } from "vite";

export default defineConfig({
  base: "./",
  plugins: [{
    name: "normalize-generated-whitespace",
    generateBundle(_options, bundle) {
      for (const output of Object.values(bundle))
        if (output.type === "chunk")
          output.code = output.code
            .replace(/^ +\t/gm, "\t")
            .replace(/[ \t]+$/gm, "");
    },
  }],
  resolve: {
    alias: {
      "@juce": resolve(import.meta.dirname, "../third_party/JUCE/modules/juce_gui_extra/native/javascript"),
    },
  },
  build: {
    outDir: resolve(import.meta.dirname, "../resources/ui"),
    emptyOutDir: true,
    sourcemap: false,
    target: "safari15",
    // The one embedded JS resource intentionally includes Three.js so JUCE never
    // needs to discover or serve a runtime chunk from an unlisted URL.
    chunkSizeWarningLimit: 900,
    rollupOptions: {
      output: {
        entryFileNames: "app.js",
        chunkFileNames: "chunk-[name].js",
        assetFileNames: ({ names }) => names.some((name) => name.endsWith(".css")) ? "app.css" : "asset-[name][extname]",
      },
    },
  },
});
