#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace mtm::ui
{
// Analoges Zeigerinstrument mit Bernstein-Hintergrundbeleuchtung.
//  Level: VU-Skala −20 … +3 (0 VU = −18 dBFS RMS), Peak-Lampe ab −1 dBFS
//  GainReduction: 0 … −20 dB (Ruhelage rechts)
class VuMeter : public juce::Component
{
public:
    enum class Mode { Level, GainReduction };

    VuMeter (juce::String caption, Mode mode);

    // regelmäßig aufrufen (UI-Timer); dt = Zeit seit letztem Aufruf
    void update (float rmsDb, float peakDb, float dtSec);
    void updateGainReduction (float grDb, float dtSec);

    void setCaption (const juce::String& c) { caption = c; repaint(); }

    void paint (juce::Graphics&) override;

private:
    float positionForVu (float vu) const;    // 0..1
    float positionForGr (float gr) const;

    juce::String caption;
    Mode mode;
    float needle = 0.0f;     // 0..1
    float peakHold = 0.0f;   // Sekunden Restleuchten
};
} // namespace mtm::ui
