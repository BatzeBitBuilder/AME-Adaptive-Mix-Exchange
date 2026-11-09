// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "Fonts.h"
#include "BinaryData.h"

namespace ame::ui::fonts
{
namespace
{
juce::Typeface::Ptr load (const char* data, int size)
{
    return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
}
} // namespace

juce::Typeface::Ptr displayTypeface()
{
    static auto tf = load (BinaryData::AbrilFatfaceRegular_ttf, BinaryData::AbrilFatfaceRegular_ttfSize);
    return tf;
}

juce::Typeface::Ptr labelTypeface()
{
    static auto tf = load (BinaryData::BebasNeueRegular_ttf, BinaryData::BebasNeueRegular_ttfSize);
    return tf;
}

juce::Typeface::Ptr monoTypeface (bool bold)
{
    static auto regular = load (BinaryData::CourierPrimeRegular_ttf, BinaryData::CourierPrimeRegular_ttfSize);
    static auto boldTf  = load (BinaryData::CourierPrimeBold_ttf, BinaryData::CourierPrimeBold_ttfSize);
    return bold ? boldTf : regular;
}

juce::FontOptions display (float height) { return juce::FontOptions (displayTypeface()).withHeight (height); }
juce::FontOptions label (float height)   { return juce::FontOptions (labelTypeface()).withHeight (height); }
juce::FontOptions mono (float height, bool bold) { return juce::FontOptions (monoTypeface (bold)).withHeight (height); }
} // namespace ame::ui::fonts
