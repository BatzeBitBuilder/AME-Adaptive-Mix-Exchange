#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/StripPanel.h"
#include "ui/MixOverview.h"
#include "ui/HandbookView.h"
#include "i18n/Lang.h"

class MultiTrackMixEditor : public juce::AudioProcessorEditor,
                            private juce::Timer,
                            private mtm::i18n::Listener
{
public:
    explicit MultiTrackMixEditor (MultiTrackMixProcessor&);
    ~MultiTrackMixEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // für Tests / Screenshots
    void showHandbook (bool shouldShow);
    mtm::ui::HandbookView& getHandbook() { return handbook; }

private:
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void languageChanged() override;
    void updateTexts();
    static void refillCombo (juce::ComboBox&, const juce::StringArray&);
    void updateVisibility();
    void paintLogo (juce::Graphics&);

    MultiTrackMixProcessor& proc;
    mtm::ui::MtmLookAndFeel lnf;

    juce::Label statusLabel;
    juce::Rectangle<int> logoArea;                  // AME-Schriftzug + Lampen
    bool lampLine = false, lampEngine = false, lampHold = false;
    static constexpr int cheek = 14, headerH = 80;
    juce::Label modeLabel, roleLabel, genreLabel, globalAmountLabel, autoRoleLabel;
    juce::ComboBox modeBox, roleBox, genreBox;
    juce::Slider globalAmount { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextButton overviewButton;
    juce::ToggleButton globalBypass;
    juce::TextButton freezeAllButton, releaseAllButton;
    juce::TextButton langDe { "DE" }, langEn { "EN" }, manualButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> globalBypassAtt;

    std::unique_ptr<ComboAttachment> modeAtt, roleAtt, genreAtt;
    std::unique_ptr<SliderAttachment> globalAmountAtt;

    mtm::ui::StripPanel strip;
    mtm::ui::MixOverview overview;
    mtm::ui::HandbookView handbook;
    juce::TooltipWindow tooltips { this, 700 };

    mtm::InstanceMode lastMode = mtm::InstanceMode::Track;
    int frame = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiTrackMixEditor)
};
