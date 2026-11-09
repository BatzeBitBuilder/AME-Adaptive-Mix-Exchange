// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

namespace ame::ui
{
// Eingebautes Handbuch: rendert eine Markdown-Teilmenge (#, ##, ###, Absätze, Listen,
// Tabellen, > Hinweise, ``` Code, ---, **fett**, `code`, [Link](url)) im Vintage-Stil.
// Inhalt: docs/manual/*.md (per BinaryData eingebettet), Sprache folgt der UI.
class HandbookView : public juce::Component
{
public:
    HandbookView();
    ~HandbookView() override;

    void loadMarkdown (const juce::String& markdown);
    void loadForCurrentLanguage();

    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;

    // für Tests
    int getNumChapters() const { return (int) chapters.size(); }
    void showChapter (int index);

    struct Block;   // intern

private:
    class Page;
    class Toc;

    std::vector<std::unique_ptr<Block>> blocks;
    struct Chapter { juce::String title; int blockIndex; };
    std::vector<Chapter> chapters;

    std::unique_ptr<Page> page;
    juce::Viewport viewport;
    std::unique_ptr<Toc> toc;
    juce::TextButton closeButton;
    juce::Rectangle<int> headerArea, tocArea;
    juce::String docTitle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HandbookView)
};

// Eingebetteten Handbuchtext liefern (de/en)
juce::String getEmbeddedManual (bool english);
} // namespace ame::ui
