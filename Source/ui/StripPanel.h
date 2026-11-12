// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <memory>
#include <vector>

#include "hub/MixHub.h"
#include "EqGraph.h"
#include "VuMeter.h"
#include "Waterfall.h"
#include "FoldButton.h"

namespace ame::ui
{
// Bedienoberfläche des Channelstrips (Spur-Modus)
//   oben:   Preset-Leiste, INPUT | FILTER | Modul-Rack (Reiter: Gate, Kompressor, De-Esser …)
//   unten:  EQ mit Analyzer + Spektrogramm (einklappbar) | ADAPTIV (einklappbar)
//   rechts: VU, Typenschild, OUTPUT
class StripPanel : public juce::Component
{
public:
    StripPanel (juce::AudioProcessorValueTreeState& state, MixHub& hub, int slot);
    ~StripPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshMeters();
    void updateTexts();   // nach Sprachwechsel

    // Rack-Reiter (für Tests / Screenshots)
    enum class Module { Gate = 0, Compressor, DeEsser, Saturation, Limiter, NumModules };
    void showModule (Module);
    Module getVisibleModule() const noexcept { return currentModule; }

    // Presets (für Tests): Werkspreset der aktuellen Rolle laden
    void loadPresetForCurrentRole();
    Role getCurrentRole() const noexcept { return currentRole; }

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };

    struct Choice
    {
        juce::ComboBox box;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    struct Section
    {
        juce::String title;
        std::vector<Knob*> knobs;
        juce::Rectangle<int> bounds;
    };

    // Modul im Rack: Reiter, Schalter/Auswahl in der Steuerzeile, Regler, Reduktionsanzeige
    struct RackModule
    {
        juce::TextButton tab;
        const char* onParam = nullptr;
        std::vector<Toggle*> toggles;
        std::vector<Choice*> choices;
        std::vector<Knob*> knobs;
        std::function<float (const SlotAudioData&)> reduction;   // dB >= 0
        float meterDb = 0.0f;
        int shownSegments = -1;   // zuletzt gezeichnete LED-Segmente
    };

    Knob* addKnob (const char* paramId, const juce::String& label);
    Toggle* addToggle (const char* paramId, const juce::String& label);
    Choice* addChoice (const char* paramId, const juce::StringArray& items);
    RackModule& addModule (const char* onParam, std::function<float (const SlotAudioData&)> reduction);
    void layoutKnobRow (const std::vector<Knob*>&, juce::Rectangle<int>);
    void layoutRack (juce::Rectangle<int>);
    void paintPanels (juce::Graphics&);   // gravierte Platten: statisch, als Bild gehalten
    void paintRack (juce::Graphics&);
    void paintAdaptive (juce::Graphics&);
    void paintPlate (juce::Graphics&);
    void fillPresetBox();
    void presetChosen();
    void updateRoleButton();
    void updatePresetButtons();
    void askSavePreset();
    void askDeletePreset();
    void applyFolds();   // Sichtbarkeit nach Klappzustand

    juce::AudioProcessorValueTreeState& apvts;
    MixHub& hub;
    int slot;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Toggle>> toggles;
    std::vector<std::unique_ptr<Choice>> choices;

    Section input, filter, eq, output, adaptive;
    Toggle *phaseLToggle = nullptr, *phaseRToggle = nullptr;
    Toggle *hpfToggle = nullptr, *lpfToggle = nullptr;
    Choice *hpfSlope = nullptr, *lpfSlope = nullptr, *lowType = nullptr, *highType = nullptr, *order = nullptr;
    juce::Label orderLabel, lowTypeLabel, highTypeLabel;
    Knob* reactionKnob = nullptr;

    // Presets
    juce::Rectangle<int> presetBar, presetTools;
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::TextButton fromRoleButton, saveButton, deleteButton, abA { "A" }, abB { "B" }, abCopy;
    Role currentRole = Role::Auto;

    // Rack
    std::vector<std::unique_ptr<RackModule>> modules;
    Module currentModule = Module::Compressor;
    juce::Rectangle<int> rackBounds, rackMeter, outputSpare;
    Toggle *gateOn = nullptr, *compOn = nullptr, *compAutoRel = nullptr, *deessOn = nullptr, *deessListen = nullptr,
           *satOn = nullptr, *limOn = nullptr;
    Choice *gateMode = nullptr, *deessMode = nullptr, *satType = nullptr, *limMode = nullptr;

    // Adaptiv
    Toggle *aEqToggle = nullptr, *aHpfToggle = nullptr, *aGainToggle = nullptr,
           *aBypassToggle = nullptr, *aFreezeToggle = nullptr;
    Choice* learnChoice = nullptr;   // Mitschnitt: Live / Aufnahme / Wiedergabe
    juce::Label learnLabel;
    int learnMode = 0;
    float learnSec = 0.0f;
    bool learnActive = false, renderWarn = false;
    EqGraph eqGraph;
    juce::Label priorityLabel;
    juce::ComboBox priorityBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> priorityAtt;
    juce::Rectangle<int> adaptiveDisplay;
    FoldButton adaptiveFold { "foldLineAdaptive" }, spectroFold { "foldLineSpectrogram" };

    std::array<float, kNumAdaptiveBands> cutDb {};
    float hpfHz = 0.0f, gainDb = 0.0f;
    bool engineAlive = false, frozen = false, bypassed = false, rawMonitor = false, noEngineMonitor = false, exchangePresent = false;

    VuMeter levelVu { {}, VuMeter::Mode::Level };
    VuMeter grVu { {}, VuMeter::Mode::GainReduction };
    Waterfall waterfall;
    juce::Rectangle<int> plateArea;
    juce::String plateName, plateRole, plateGenre, plateLine;
    int plateCounter = 14;

    BackdropCache backdrop;
    // Neuzeichnen nur bei sichtbaren Änderungen; ohne Host-Blöcke (~1,5 s) ruhen Spektrum und Spektrogramm
    uint32_t lastBlockCounter = 0;
    int idleTicks = 0;
    std::array<int, kNumAdaptiveBands + 5> adaptiveShown {};
    static int rackSegments (float db) noexcept;
};
} // namespace ame::ui
