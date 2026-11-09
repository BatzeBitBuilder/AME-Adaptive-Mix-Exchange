// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "Settings.h"

#include <juce_data_structures/juce_data_structures.h>

namespace ame::settings
{
namespace
{
juce::File& testFolder()
{
    static juce::File f;
    return f;
}

juce::PropertiesFile::Options options()
{
    juce::PropertiesFile::Options o;
    o.applicationName     = "AME";
    o.folderName          = "AME";
    o.filenameSuffix      = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    o.storageFormat       = juce::PropertiesFile::storeAsXML;
    return o;
}

std::unique_ptr<juce::PropertiesFile> open()
{
    if (testFolder() != juce::File())
        return std::make_unique<juce::PropertiesFile> (testFolder().getChildFile ("AME.settings"), options());
    return std::make_unique<juce::PropertiesFile> (options());
}
} // namespace

juce::String get (const juce::String& key, const juce::String& fallback)
{
    try { return open()->getValue (key, fallback); }
    catch (...) { return fallback; }
}

void set (const juce::String& key, const juce::String& value)
{
    auto f = open();
    f->setValue (key, value);
    f->saveIfNeeded();
}

juce::File getUserDataFolder()
{
    if (testFolder() != juce::File())
        return testFolder();
    return options().getDefaultFile().getParentDirectory();
}

void setUserDataFolderForTesting (const juce::File& f)
{
    testFolder() = f;
}
} // namespace ame::settings
