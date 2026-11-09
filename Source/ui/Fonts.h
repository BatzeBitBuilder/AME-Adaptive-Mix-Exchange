// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_graphics/juce_graphics.h>

// Eingebettete Vintage-Schriften (OFL):
//   display = Abril Fatface (Namensschild), label = Bebas Neue (Gravur), mono = Courier Prime (Werte, Anzeigen)
namespace ame::ui::fonts
{
juce::Typeface::Ptr displayTypeface();
juce::Typeface::Ptr labelTypeface();
juce::Typeface::Ptr monoTypeface (bool bold = false);

juce::FontOptions display (float height);
juce::FontOptions label (float height);
juce::FontOptions mono (float height, bool bold = false);
} // namespace ame::ui::fonts
