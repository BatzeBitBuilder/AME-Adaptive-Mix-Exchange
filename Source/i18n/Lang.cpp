#include "Lang.h"

#include <juce_data_structures/juce_data_structures.h>
#include <atomic>

namespace mtm::i18n
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

juce::PropertiesFile::Options settingsOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName     = "AME";
    o.folderName          = "AME";
    o.filenameSuffix      = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    o.storageFormat       = juce::PropertiesFile::storeAsXML;
    return o;
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
        try
        {
            juce::PropertiesFile file (settingsOptions());
            const auto stored = file.getValue (kKey);
            if (stored == "en")      l = Language::English;
            else if (stored == "de") l = Language::German;
        }
        catch (...) {}
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

    juce::PropertiesFile file (settingsOptions());
    file.setValue (kKey, l == Language::English ? "en" : "de");
    file.saveIfNeeded();

    notify();
}

void setLanguageForTesting (Language l)
{
    state().store ((int) l, std::memory_order_relaxed);
    notify();
}

void addListener (Listener* l)    { listeners().add (l); }
void removeListener (Listener* l) { listeners().remove (l); }
} // namespace mtm::i18n
