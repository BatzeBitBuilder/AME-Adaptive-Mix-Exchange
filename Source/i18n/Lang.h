#pragma once

#include <juce_core/juce_core.h>

// UTF-8-Literal -> juce::String (Umlaute)
#ifndef MTM_TXT
 #define MTM_TXT(s) juce::String (juce::CharPointer_UTF8 (s))
#endif

namespace mtm::i18n
{
enum class Language { German = 0, English };

// Aktuelle UI-Sprache (prozessweit, für alle Instanzen gleich).
// Beim ersten Zugriff aus der Einstellungsdatei geladen, sonst Systemsprache.
Language current() noexcept;
inline bool isEnglish() noexcept { return current() == Language::English; }

// Nur vom Message-Thread aufrufen: speichert und benachrichtigt alle Listener synchron.
void setLanguage (Language);

// Nur für Tests: Sprache setzen, ohne Datei zu schreiben.
void setLanguageForTesting (Language);

struct Listener
{
    virtual ~Listener() = default;
    virtual void languageChanged() = 0;
};
void addListener (Listener*);
void removeListener (Listener*);

// Text in der aktuellen Sprache (beide Literale UTF-8)
inline juce::String tr (const char* de, const char* en)
{
    return MTM_TXT (isEnglish() ? en : de);
}
} // namespace mtm::i18n
