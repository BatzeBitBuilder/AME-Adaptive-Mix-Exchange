// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_core/juce_core.h>

// Benutzer-Einstellungen (global für alle Instanzen): %APPDATA%\AME\AME.settings
namespace mtm::settings
{
juce::String get (const juce::String& key, const juce::String& fallback = {});
void set (const juce::String& key, const juce::String& value);

juce::File getUserDataFolder();                     // %APPDATA%\AME
void setUserDataFolderForTesting (const juce::File&);
}
