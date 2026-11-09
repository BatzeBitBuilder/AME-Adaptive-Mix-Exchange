// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/StripPanel.h"
#include "ui/MixOverview.h"
#include "ui/HandbookView.h"
#include "i18n/Lang.h"

// Gesamte Oberfläche in Basisgröße; der Editor skaliert sie (Fenster frei skalierbar)
class MainView : public juce::Component,
                 private juce::Timer,
                 private ame::i18n::Listener
{
public:
    static constexpr int baseWidth = 1400, baseHeight = 760;   // Querformat, passt auch bei 1080p mit 125 % Windows-Skalierung

    explicit MainView (AmeProcessor&);
    ~MainView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // für Tests / Screenshots
    void showHandbook (bool shouldShow);
    ame::ui::HandbookView& getHandbook() { return handbook; }
    ame::ui::StripPanel& getStrip() { return strip; }

private:
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void languageChanged() override;
    void updateTexts();
    static void refillCombo (juce::ComboBox&, const juce::StringArray&);
    void updateVisibility();
    void paintLogo (juce::Graphics&);

    AmeProcessor& proc;
    ame::ui::AmeLookAndFeel lnf;

    juce::Label statusLabel;
    juce::Rectangle<int> logoArea;                  // AME-Schriftzug + Lampen
    bool lampLine = false, lampEngine = false, lampHold = false;
    static constexpr int cheek = 14, headerH = 80;
    juce::Label modeLabel, roleLabel, genreLabel, globalAmountLabel, autoRoleLabel;
    juce::ComboBox modeBox, roleBox, genreBox;
    juce::Slider globalAmount { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextButton overviewButton;
    juce::ToggleButton globalBypass;   // Altparameter, nicht mehr sichtbar (ersetzt durch die Abhöre)
    juce::Label monitorLabel;
    juce::TextButton monitorFull, monitorNoEngine, monitorRaw;
    juce::ToggleButton levelMatch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> levelMatchAtt;
    juce::TextButton freezeAllButton, releaseAllButton;
    juce::Label learnBarLabel, learnStatus;                 // Exchange: Mitschnitt aller Lines
    juce::TextButton learnRecAll, learnPlayAll, learnLiveAll, learnClearAll;
    juce::Rectangle<int> learnBar;
    juce::TextButton langDe { "DE" }, langEn { "EN" }, manualButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> globalBypassAtt;

    std::unique_ptr<ComboAttachment> modeAtt, roleAtt, genreAtt;
    std::unique_ptr<SliderAttachment> globalAmountAtt;

    ame::ui::StripPanel strip;
    ame::ui::MixOverview overview;
    ame::ui::HandbookView handbook;
    juce::TooltipWindow tooltips { this, 700 };

    ame::InstanceMode lastMode = ame::InstanceMode::Track;
    int frame = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};

class AmeEditor : public juce::AudioProcessorEditor
{
public:
    explicit AmeEditor (AmeProcessor&);
    ~AmeEditor() override;

    void resized() override;

    // für Tests / Screenshots
    void showHandbook (bool shouldShow)        { view.showHandbook (shouldShow); }
    ame::ui::HandbookView& getHandbook()       { return view.getHandbook(); }
    ame::ui::StripPanel& getStrip()            { return view.getStrip(); }
    float getScale() const noexcept            { return scale; }

private:
    MainView view;
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmeEditor)
};
