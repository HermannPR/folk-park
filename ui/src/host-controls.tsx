import { useEffect, useMemo, useState } from "react";
import { getComboBoxState, getSliderState, getToggleState } from "@juce/index.js";
import { Dropdown, Knob, Toggle } from "./design-system.tsx";

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
  const tone = relay.includes("filter") || relay.includes("eq") ? "lime"
    : relay.includes("reverb") || relay.includes("delay") ? "turquoise"
      : relay.includes("drive") || relay.includes("dist") ? "orange" : "violet";
  return <Knob className="host-control" id={id} normalized={normalized} tone={tone}
    label={label} output={<>{scaled.toFixed(decimals)}{state.properties.label ? ` ${state.properties.label}` : ""}</>}
    min="0" max="1" step="0.001" value={normalized}
      onPointerDown={() => state.sliderDragStarted()}
      onPointerUp={() => state.sliderDragEnded()}
      onPointerCancel={() => state.sliderDragEnded()}
      onChange={(event) => state.setNormalisedValue(Number(event.currentTarget.value))} />;
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
  return <Dropdown className="host-control" id={id} label={label} value={choice}
    onChange={(event) => state.setChoiceIndex(Number(event.currentTarget.value))}>
      {choices.map((name, index) => <option key={`${name}-${index}`} value={index}>{name}</option>)}
  </Dropdown>;
}

export function HostToggle({ id, relay, label }: ComboProps) {
  const state = useMemo(() => getToggleState(relay), [relay]);
  const [checked, setChecked] = useState(state.getValue());
  useEffect(() => {
    const update = () => setChecked(state.getValue());
    const valueListener = state.valueChangedEvent.addListener(update);
    const propertyListener = state.propertiesChangedEvent.addListener(update);
    update();
    return () => {
      state.valueChangedEvent.removeListener(valueListener);
      state.propertiesChangedEvent.removeListener(propertyListener);
    };
  }, [state]);
  return <Toggle className="host-control host-toggle" id={id} label={label}
    output={checked ? "Bypassed" : "Active"} checked={checked}
    onChange={(event) => state.setValue(event.currentTarget.checked)} />;
}
