import { useEffect, useMemo, useState } from "react";
import { getComboBoxState, getSliderState } from "@juce/index.js";

type SliderProps = {
  id: string;
  relay: string;
  label: string;
  decimals?: number;
};

export function useHostNormalized(relay: string) {
  const state = useMemo(() => getSliderState(relay), [relay]);
  const [normalized, setNormalized] = useState(state.getNormalisedValue());
  useEffect(() => {
    const update = () => setNormalized(state.getNormalisedValue());
    const valueListener = state.valueChangedEvent.addListener(update);
    const propertyListener = state.propertiesChangedEvent.addListener(update);
    update();
    return () => {
      state.valueChangedEvent.removeListener(valueListener);
      state.propertiesChangedEvent.removeListener(propertyListener);
    };
  }, [state]);
  return normalized;
}

export function HostSlider({ id, relay, label, decimals = 2 }: SliderProps) {
  const state = useMemo(() => getSliderState(relay), [relay]);
  const [normalized, setNormalized] = useState(state.getNormalisedValue());
  const [scaled, setScaled] = useState(state.getScaledValue());
  const update = () => {
    setNormalized(state.getNormalisedValue());
    setScaled(state.getScaledValue());
  };
  useEffect(() => {
    const valueListener = state.valueChangedEvent.addListener(update);
    const propertyListener = state.propertiesChangedEvent.addListener(update);
    update();
    return () => {
      state.valueChangedEvent.removeListener(valueListener);
      state.propertiesChangedEvent.removeListener(propertyListener);
    };
  }, [state]);
  return <label className="host-control" htmlFor={id}>
    <span>{label}<output>{scaled.toFixed(decimals)}{state.properties.label ? ` ${state.properties.label}` : ""}</output></span>
    <input id={id} type="range" min="0" max="1" step="0.001" value={normalized}
      onPointerDown={() => state.sliderDragStarted()}
      onPointerUp={() => state.sliderDragEnded()}
      onPointerCancel={() => state.sliderDragEnded()}
      onChange={(event) => state.setNormalisedValue(Number(event.currentTarget.value))} />
  </label>;
}

type ComboProps = { id: string; relay: string; label: string };

export function HostCombo({ id, relay, label }: ComboProps) {
  const state = useMemo(() => getComboBoxState(relay), [relay]);
  const [choice, setChoice] = useState(state.getChoiceIndex());
  const [choices, setChoices] = useState([...state.properties.choices]);
  useEffect(() => {
    const valueListener = state.valueChangedEvent.addListener(() => setChoice(state.getChoiceIndex()));
    const propertyListener = state.propertiesChangedEvent.addListener(() => {
      setChoices([...state.properties.choices]);
      setChoice(state.getChoiceIndex());
    });
    return () => {
      state.valueChangedEvent.removeListener(valueListener);
      state.propertiesChangedEvent.removeListener(propertyListener);
    };
  }, [state]);
  return <label className="host-control" htmlFor={id}>
    <span>{label}</span>
    <select id={id} value={choice} onChange={(event) => state.setChoiceIndex(Number(event.currentTarget.value))}>
      {choices.map((name, index) => <option key={`${name}-${index}`} value={index}>{name}</option>)}
    </select>
  </label>;
}
