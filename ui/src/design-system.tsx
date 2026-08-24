import type { ButtonHTMLAttributes, CSSProperties, HTMLAttributes, InputHTMLAttributes,
  ReactNode, SelectHTMLAttributes, TextareaHTMLAttributes } from "react";

type Tone = "violet" | "lime" | "pink" | "orange" | "turquoise" | "blue";

const classes = (...values: Array<string | false | null | undefined>) =>
  values.filter(Boolean).join(" ");

export function Button({ className, ...props }: ButtonHTMLAttributes<HTMLButtonElement>) {
  return <button className={classes("fp-button", className)} {...props} />;
}

export function IconButton({ className, "aria-label": ariaLabel, children, ...props }:
  ButtonHTMLAttributes<HTMLButtonElement> & { "aria-label": string }) {
  return <button className={classes("fp-icon-button", className)} aria-label={ariaLabel}
    {...props}>{children}</button>;
}

export function Panel({ className, children, ...props }: HTMLAttributes<HTMLElement>) {
  return <section className={classes("fp-panel", className)} {...props}>{children}</section>;
}

export function Sidebar({ className, children, ...props }: HTMLAttributes<HTMLElement>) {
  return <nav className={classes("fp-sidebar", className)} {...props}>{children}</nav>;
}

export function Navbar({ className, children, ...props }: HTMLAttributes<HTMLElement>) {
  return <header className={classes("fp-navbar", className)} {...props}>{children}</header>;
}

export function Tabs({ className, children, ...props }: HTMLAttributes<HTMLDivElement>) {
  return <div className={classes("fp-tabs", className)} role="tablist" {...props}>{children}</div>;
}

type SliderProps = Omit<InputHTMLAttributes<HTMLInputElement>, "type"> & {
  label: ReactNode;
  output?: ReactNode;
  tone?: Tone;
};

export function Slider({ label, output, tone = "violet", className, ...props }: SliderProps) {
  return <label className={classes("fp-slider", `tone-${tone}`, className)}>
    <span className="fp-control-label">{label}{output !== undefined && <output>{output}</output>}</span>
    <span className="fp-slider-rod"><input type="range" {...props} /></span>
  </label>;
}

type KnobProps = SliderProps & { normalized: number };

export function Knob({ label, output, normalized, tone = "violet", className, ...props }: KnobProps) {
  const bounded = Math.max(0, Math.min(1, normalized));
  const style = { "--knob-angle": `${-135 + bounded * 270}deg`,
    "--knob-value": bounded } as CSSProperties;
  return <label className={classes("fp-knob", `tone-${tone}`, className)} style={style}>
    <span className="fp-control-label">{label}</span>
    <span className="fp-knob-stage" aria-hidden="true"><i className="fp-knob-face"><b /></i></span>
    <input className="fp-knob-input" type="range" {...props} />
    {output !== undefined && <output>{output}</output>}
  </label>;
}

type ToggleProps = Omit<InputHTMLAttributes<HTMLInputElement>, "type"> & {
  label: ReactNode;
  output?: ReactNode;
  tone?: Tone;
};

export function Toggle({ label, output, tone = "lime", className, ...props }: ToggleProps) {
  return <label className={classes("fp-toggle", `tone-${tone}`, className)}>
    <input type="checkbox" {...props} />
    <span className="fp-toggle-track" aria-hidden="true"><i /></span>
    <span className="fp-toggle-copy"><strong>{label}</strong>{output !== undefined && <small>{output}</small>}</span>
  </label>;
}

type DropdownProps = SelectHTMLAttributes<HTMLSelectElement> & {
  label: ReactNode;
};

export function Dropdown({ label, className, children, ...props }: DropdownProps) {
  return <label className={classes("fp-field", "fp-dropdown", className)}>
    <span>{label}</span><span className="fp-select-shell"><select {...props}>{children}</select><i>⌄</i></span>
  </label>;
}

type TextInputProps = InputHTMLAttributes<HTMLInputElement> & { label: ReactNode };

export function TextInput({ label, className, ...props }: TextInputProps) {
  return <label className={classes("fp-field", className)}><span>{label}</span><input {...props} /></label>;
}

export function NumericInput({ label, className, ...props }: TextInputProps) {
  return <TextInput label={label} type="number" className={classes("fp-numeric", className)} {...props} />;
}

export function TextArea({ label, className, ...props }:
  TextareaHTMLAttributes<HTMLTextAreaElement> & { label: ReactNode }) {
  return <label className={classes("fp-field", className)}><span>{label}</span><textarea {...props} /></label>;
}

export function ProgressBar({ value, label }: { value: number; label: string }) {
  const bounded = Math.max(0, Math.min(1, value));
  return <div className="fp-progress" role="progressbar" aria-label={label} aria-valuemin={0}
    aria-valuemax={100} aria-valuenow={Math.round(bounded * 100)}><i style={{ width: `${bounded * 100}%` }} /></div>;
}

export function Meter({ value, label, tone = "lime" }:
  { value: number; label: string; tone?: Tone }) {
  const bounded = Math.max(0, Math.min(1, value));
  return <div className={classes("fp-meter", `tone-${tone}`)} aria-label={label} role="meter"
    aria-valuemin={0} aria-valuemax={100} aria-valuenow={Math.round(bounded * 100)}>
    {Array.from({ length: 10 }, (_, index) => <i key={index}
      className={index / 10 < bounded ? "lit" : ""} />)}
  </div>;
}

export function StatusIndicator({ children, good = false, tone = "orange" }:
  { children: ReactNode; good?: boolean; tone?: Tone }) {
  return <span className={classes("fp-status", `tone-${good ? "lime" : tone}`)}><i />{children}</span>;
}

export function Tooltip({ text, children }: { text: string; children: ReactNode }) {
  return <span className="fp-tooltip" data-tooltip={text}>{children}</span>;
}

export function Modal({ open, title, children, onClose }:
  { open: boolean; title: string; children: ReactNode; onClose: () => void }) {
  if (!open) return null;
  return <div className="fp-modal-backdrop" role="presentation" onMouseDown={(event) => {
    if (event.target === event.currentTarget) onClose();
  }}><section className="fp-modal" role="dialog" aria-modal="true" aria-label={title}>
    <header><h2>{title}</h2><IconButton aria-label="Close dialog" onClick={onClose}>×</IconButton></header>
    {children}
  </section></div>;
}

export function ContextMenu({ open, children }: { open: boolean; children: ReactNode }) {
  if (!open) return null;
  return <div className="fp-context-menu" role="menu">{children}</div>;
}

export function Notification({ children, className, ...props }: HTMLAttributes<HTMLDivElement>) {
  return <div className={classes("fp-notification", className)} {...props}><i />{children}</div>;
}
