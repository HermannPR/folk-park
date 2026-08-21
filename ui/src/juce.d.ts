declare module "@juce/index.js" {
  type Listener<T = void> = (value: T) => void;
  type ListenerList<T = void> = {
    addListener(listener: Listener<T>): number;
    removeListener(identifier: number): void;
  };
  export type SliderState = {
    properties: { label?: string; choices?: string[] };
    valueChangedEvent: ListenerList;
    propertiesChangedEvent: ListenerList;
    getNormalisedValue(): number;
    getScaledValue(): number;
    setNormalisedValue(value: number): void;
    sliderDragStarted(): void;
    sliderDragEnded(): void;
  };
  export type ComboBoxState = {
    properties: { choices: string[] };
    valueChangedEvent: ListenerList;
    propertiesChangedEvent: ListenerList;
    getChoiceIndex(): number;
    setChoiceIndex(value: number): void;
  };
  export type ToggleState = {
    valueChangedEvent: ListenerList;
    propertiesChangedEvent: ListenerList;
    getValue(): boolean;
    setValue(value: boolean): void;
  };
  export function getNativeFunction(name: string): (...arguments_: unknown[]) => Promise<unknown>;
  export function getSliderState(name: string): SliderState;
  export function getComboBoxState(name: string): ComboBoxState;
  export function getToggleState(name: string): ToggleState;
}

type JuceBackend = {
  addEventListener(name: string, listener: (payload: unknown) => void): number;
  removeEventListener?(name: string, identifier: number): void;
};

interface Window {
  __JUCE__: { backend: JuceBackend };
}
