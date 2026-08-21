import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import App from "./App.tsx";
import "./styles.css";

const root = document.querySelector("#root");
if (root === null) throw new Error("folk park UI root is missing");
createRoot(root).render(<StrictMode><App /></StrictMode>);
