// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "Lang.h"

#include "Settings.h"
#include <atomic>

namespace ame::i18n
{
namespace
{
constexpr const char* kKey = "language";

std::atomic<int>& state()
{
    static std::atomic<int> lang { -1 };   // -1 = noch nicht geladen
    return lang;
}

juce::ListenerList<Listener>& listeners()
{
    static juce::ListenerList<Listener> l;
    return l;
}

Language systemDefault()
{
    return juce::SystemStats::getUserLanguage().startsWithIgnoreCase ("de") ? Language::German : Language::English;
}

void notify()
{
    listeners().call ([] (Listener& l) { l.languageChanged(); });
}
} // namespace

Language current() noexcept
{
    auto& s = state();
    int v = s.load (std::memory_order_relaxed);
    if (v < 0)
    {
        Language l = systemDefault();
        const auto stored = settings::get (kKey);
        if (stored == "en")      l = Language::English;
        else if (stored == "de") l = Language::German;
        int expected = -1;
        s.compare_exchange_strong (expected, (int) l);
        v = s.load (std::memory_order_relaxed);
    }
    return (Language) v;
}

void setLanguage (Language l)
{
    if (current() == l)
        return;

    state().store ((int) l, std::memory_order_relaxed);

    settings::set (kKey, l == Language::English ? "en" : "de");

    notify();
}

void setLanguageForTesting (Language l)
{
    state().store ((int) l, std::memory_order_relaxed);
    notify();
}

void addListener (Listener* l)    { listeners().add (l); }
void removeListener (Listener* l) { listeners().remove (l); }
} // namespace ame::i18n
