#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "hub/MaskingModel.h"

namespace mtm::ui
{
// Matrix: Zeile = verdeckte Spur, Spalte = verdeckende Spur, Farbe = Stärke (gelb -> rot)
class MaskingMatrix : public juce::Component
{
public:
    std::function<void (uint32_t instanceId)> onSelect;

    void setData (const std::vector<TrackSnapshot>& tracks, const MaskingResult& masking,
                  const std::vector<juce::Colour>& trackColours, uint32_t selectedId);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> gridArea() const;

    std::vector<juce::String> names;
    std::vector<juce::Colour> cols;
    std::vector<uint32_t> ids;
    std::vector<float> values;   // n * n
    int n = 0;
    uint32_t selected = 0;
};
} // namespace mtm::ui
