// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

#include "hub/MixHub.h"
#include "EqGraph.h"
#include "VuMeter.h"
#include "Waterfall.h"

namespace mtm::ui
{
// Bedienoberfläche des Channelstrips (Spur-Modus)
class StripPanel : public juce::Component
{
public:
    StripPanel (juce::AudioProcessorValueTreeState& state, MixHub& hub, int slot);
    ~StripPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshMeters();
    void updateTexts();   // nach Sprachwechsel

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

    struct Section
    {
        Section (juce::String t, std::vector<Knob*> k, Toggle* tg = nullptr)
            : title (std::move (t)), knobs (std::move (k)), toggle (tg) {}

        juce::String title;
        std::vector<Knob*> knobs;
        Toggle* toggle = nullptr;
        juce::Rectangle<int> bounds;
    };

    Knob* addKnob (const char* paramId, const juce::String& label);
    Toggle* addToggle (const char* paramId, const juce::String& label);
    void layoutKnobs (Section&, juce::Rectangle<int>);
    void paintAdaptive (juce::Graphics&);

    juce::AudioProcessorValueTreeState& apvts;
    MixHub& hub;
    int slot;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Toggle>> toggles;
    std::vector<Section> sections;

    Toggle *hpfToggle = nullptr, *lpfToggle = nullptr, *compToggle = nullptr;
    Knob* reactionKnob = nullptr;

    // Adaptiv
    Section* adaptive = nullptr;
    Toggle *aEqToggle = nullptr, *aHpfToggle = nullptr, *aGainToggle = nullptr,
           *aBypassToggle = nullptr, *aFreezeToggle = nullptr;
    EqGraph eqGraph;
    juce::Label priorityLabel;
    juce::ComboBox priorityBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> priorityAtt;
    juce::Rectangle<int> adaptiveDisplay;

    std::array<float, kNumAdaptiveBands> cutDb {};
    float hpfHz = 0.0f, gainDb = 0.0f;
    bool engineAlive = false, frozen = false, bypassed = false;

    VuMeter levelVu { {}, VuMeter::Mode::Level };
    VuMeter grVu { {}, VuMeter::Mode::GainReduction };
    Waterfall waterfall;
    juce::Rectangle<int> plateArea;
    juce::String plateName, plateRole, plateGenre, plateLine;
    int plateCounter = 14;
};
} // namespace mtm::ui
