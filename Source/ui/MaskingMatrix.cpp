#include "MaskingMatrix.h"
#include "LookAndFeel.h"
#include "i18n/Lang.h"

namespace mtm::ui
{
void MaskingMatrix::setData (const std::vector<TrackSnapshot>& tracks, const MaskingResult& masking,
                             const std::vector<juce::Colour>& trackColours, uint32_t selectedId)
{
    n = masking.numTracks();
    selected = selectedId;
    names.clear(); cols.clear(); ids.clear();
    values.assign ((size_t) (n * n), 0.0f);

    for (int t = 0; t < n; ++t)
    {
        const int idx = masking.trackIndex[(size_t) t];
        names.push_back (tracks[(size_t) idx].name);
        cols.push_back (trackColours[(size_t) idx]);
        ids.push_back (tracks[(size_t) idx].instanceId);
    }

    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i)
        {
            if (i == j) continue;
            float sum = 0.0f;
            for (int b = 0; b < kNumAnalysisBands; ++b)
                sum += masking.get (i, j, b);
            values[(size_t) (j * n + i)] = juce::jlimit (0.0f, 1.0f, sum / 6.0f);   // ~6 voll verdeckte Bänder = Maximum
        }
    repaint();
}

juce::Rectangle<float> MaskingMatrix::gridArea() const
{
    auto r = getLocalBounds().toFloat().reduced (8.0f);
    r.removeFromTop (20.0f);    // Titel
    r.removeFromLeft (70.0f);   // Zeilennamen
    r.removeFromTop (16.0f);    // Spaltenköpfe
    r.removeFromBottom (16.0f); // Fußzeile
    const float side = juce::jmin (r.getWidth(), r.getHeight());
    return r.withSize (side, side);
}

void MaskingMatrix::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    drawEngravedPanel (g, bounds, i18n::tr ("Patchfeld", "Patchbay"));

    const auto grid = gridArea();
    drawDisplayBezel (g, grid);

    if (n < 2)
    {
        g.setFont (fonts::mono (12.0f));
        g.setColour (colours::phosphorDim);
        g.drawFittedText (i18n::tr ("mind. 2 Spuren\nmit Signal", "at least 2 tracks\nwith signal"), grid.toNearestInt(), juce::Justification::centred, 2);
        return;
    }

    const float cell = grid.getWidth() / (float) n;

    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i)
        {
            auto c = juce::Rectangle<float> (grid.getX() + cell * (float) i, grid.getY() + cell * (float) j, cell, cell).reduced (1.0f);
            if (i == j)
            {
                g.setColour (colours::displayGrid);
                g.fillRect (c);
                continue;
            }
            const float v = values[(size_t) (j * n + i)];
            g.setColour (v < 0.02f ? colours::display.brighter (0.15f) : maskingColour (v).withAlpha (0.35f + 0.65f * v));
            g.fillRect (c);
            if (cell > 22.0f && v >= 0.05f)
            {
                g.setColour (colours::display);
                g.setFont (fonts::mono (juce::jmin (12.0f, cell * 0.4f), true));
                g.drawText (juce::String (juce::roundToInt (v * 100.0f)), c, juce::Justification::centred);
            }
        }

    // Beschriftung: Zeilen (verdeckt) links, Spalten (verdeckt durch) oben als Farbmarken + Nummer
    g.setFont (fonts::mono (11.0f, true));
    for (int t = 0; t < n; ++t)
    {
        const bool sel = ids[(size_t) t] == selected;
        auto row = juce::Rectangle<float> (grid.getX() - 70.0f, grid.getY() + cell * (float) t, 66.0f, cell);
        g.setColour (cols[(size_t) t]);
        g.fillRect (row.removeFromRight (4.0f).reduced (0.0f, 2.0f));
        g.setColour (sel ? colours::rust : colours::ink);
        g.drawText (juce::String (t + 1) + " " + names[(size_t) t], row.reduced (2.0f, 0.0f), juce::Justification::centredRight, true);

        auto col = juce::Rectangle<float> (grid.getX() + cell * (float) t, grid.getY() - 16.0f, cell, 14.0f);
        g.setColour (cols[(size_t) t]);
        g.fillRect (col.removeFromBottom (3.0f).reduced (2.0f, 0.0f));
        g.setColour (colours::ink);
        g.drawText (juce::String (t + 1), col, juce::Justification::centred);
    }

    g.setFont (fonts::label (13.0f));
    g.setColour (colours::inkDim);
    g.drawText (i18n::tr ("Zeile wird von Spalte verdeckt", "row is masked by column"),
                juce::Rectangle<float> (bounds.getX() + 8.0f, grid.getBottom() + 2.0f, bounds.getWidth() - 16.0f, 14.0f),
                juce::Justification::centredRight);
}

void MaskingMatrix::mouseDown (const juce::MouseEvent& e)
{
    const auto grid = gridArea();
    if (n < 2 || onSelect == nullptr) return;
    const float cell = grid.getWidth() / (float) n;
    const int row = (int) ((e.position.y - grid.getY()) / cell);
    if (e.position.x < grid.getRight() && juce::isPositiveAndBelow (row, n))
        onSelect (ids[(size_t) row]);
}
} // namespace mtm::ui
