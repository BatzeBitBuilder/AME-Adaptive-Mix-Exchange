#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

#include "hub/MixHub.h"

namespace mtm::ui
{
// EQ-Grafik der Spur: eigenes Spektrum (vor/nach Engine), manuelle EQ-Kurve mit ziehbaren
// Punkten, adaptive Kurve der Engine.
//   Ziehen = Frequenz/Gain, Mausrad = Q (Mid-Bänder), Doppelklick = Gain 0
class EqGraph : public juce::Component
{
public:
    EqGraph (juce::AudioProcessorValueTreeState& state, MixHub& hub, int slot);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void refresh (float dtSec = 1.0f / 30.0f);
    const std::array<float, kNumDisplayPoints>& getSpectrum() const noexcept { return spectrumDb; }

private:
    struct Node
    {
        const char* freqId;
        const char* gainId;   // nullptr = nur Frequenz (HPF/LPF)
        const char* qId;      // nullptr = kein Q
        const char* onId;     // nullptr = immer aktiv
        const char* label;
    };

    juce::Rectangle<float> plotArea() const;
    float xForFreq (float hz) const;
    float freqForX (float x) const;
    float yForDb (float db) const;
    float dbForY (float y) const;
    float ySpectrum (float db) const;

    float value (const char* id) const;
    void setValue (const char* id, float v);
    bool nodeEnabled (const Node&) const;
    juce::Point<float> nodePosition (const Node&) const;
    int hitNode (juce::Point<float>) const;
    float manualResponseDb (float hz) const;

    juce::AudioProcessorValueTreeState& apvts;
    MixHub& hub;
    int slot;

    std::array<Node, 6> nodes;
    int dragNode = -1, hoverNode = -1;

    std::array<float, kNumDisplayPoints> spectrumDb {}, peakDb {};
    std::array<float, kNumAdaptiveBands> cutDb {};
    float hpfHz = 0.0f, gainDb = 0.0f;
    bool hasSpectrum = false;

    static constexpr float rangeDb = 18.0f;
};
} // namespace mtm::ui
