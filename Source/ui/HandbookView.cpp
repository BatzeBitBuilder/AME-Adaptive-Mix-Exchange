// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "HandbookView.h"
#include "LookAndFeel.h"
#include "Fonts.h"
#include "i18n/Lang.h"
#include "BinaryData.h"

namespace mtm::ui
{
using i18n::tr;

namespace
{
constexpr float bodySize = 15.5f;
constexpr int tocW = 210, headerH = 44, pagePad = 28;

struct Style
{
    juce::FontOptions font;
    juce::Colour colour;
};

Style styleFor (bool bold, bool code, float size, juce::Colour base)
{
    if (code)
        return { fonts::mono (size, true), colours::rust };
    return { fonts::mono (size, bold), base };
}

// Inline-Markdown -> AttributedString
juce::AttributedString toAttributed (const juce::String& text, float size, juce::Colour base, bool forceBold = false)
{
    juce::AttributedString as;
    as.setWordWrap (juce::AttributedString::byWord);
    as.setLineSpacing (size * 0.28f);

    bool bold = false, code = false;
    juce::String run;

    auto flush = [&] (bool linkStyle = false)
    {
        if (run.isEmpty()) return;
        auto st = styleFor (bold || forceBold, code, size, base);
        as.append (run, juce::Font (st.font), linkStyle ? colours::rust : st.colour);
        run.clear();
    };

    auto t = text.getCharPointer();
    while (! t.isEmpty())
    {
        const auto c = *t;

        if (! code && c == '*' && *(t + 1) == '*')
        {
            flush();
            bold = ! bold;
            t += 2;
            continue;
        }
        if (c == '`')
        {
            flush();
            code = ! code;
            ++t;
            continue;
        }
        if (! code && c == '[')
        {
            // [Text](url) -> Text
            const auto rest = juce::String (t);
            const int close = rest.indexOf ("](");
            const int end = close > 0 ? rest.indexOfChar (close, ')') : -1;
            if (close > 0 && end > close)
            {
                flush();
                run = rest.substring (1, close);
                flush (true);
                t += end + 1;
                continue;
            }
        }
        if (! code && c == '\\' && ! (t + 1).isEmpty())
        {
            ++t;
            run += juce::String::charToString (*t);
            ++t;
            continue;
        }

        run += juce::String::charToString (c);
        ++t;
    }
    flush();
    return as;
}

juce::StringArray splitRow (juce::String line)
{
    line = line.trim();
    if (line.startsWithChar ('|')) line = line.substring (1);
    if (line.endsWithChar ('|'))   line = line.dropLastCharacters (1);
    juce::StringArray cells;
    cells.addTokens (line, "|", {});
    for (auto& c : cells) c = c.trim();
    return cells;
}

bool isTableSeparator (const juce::String& line)
{
    const auto t = line.trim();
    return t.startsWithChar ('|') && t.containsOnly ("|-: ");
}
} // namespace

//==============================================================================
struct HandbookView::Block
{
    enum class Type { H1, H2, H3, Para, Bullet, Numbered, Quote, Code, Rule, Table } type = Type::Para;
    juce::String text, marker;
    int indent = 0;
    std::vector<juce::StringArray> rows;

    // Layout
    int y = 0, h = 0;
    std::vector<juce::TextLayout> layouts;
    std::vector<juce::Rectangle<float>> rects;
    std::vector<float> colWidths;
};

//==============================================================================
class HandbookView::Page : public juce::Component
{
public:
    explicit Page (std::vector<std::unique_ptr<Block>>& b) : blocks (b) {}

    int layout (int width)
    {
        const float w = (float) (width - 2 * pagePad);
        float y = (float) pagePad;

        for (auto& bp : blocks)
        {
            auto& b = *bp;
            b.layouts.clear();
            b.rects.clear();

            auto single = [&] (const juce::AttributedString& as, float x, float maxW)
            {
                juce::TextLayout tl;
                tl.createLayout (as, maxW);
                b.layouts.push_back (std::move (tl));
                b.rects.push_back ({ x, 0.0f, maxW, b.layouts.back().getHeight() });
                return b.layouts.back().getHeight();
            };

            using T = Block::Type;
            float top = 0.0f, hgt = 0.0f, after = 10.0f;

            switch (b.type)
            {
                case T::H1:
                {
                    juce::AttributedString as;
                    as.setWordWrap (juce::AttributedString::byWord);
                    as.append (b.text, juce::Font (fonts::display (34.0f)), colours::ink);
                    hgt = single (as, 0.0f, w);
                    after = 6.0f;
                    break;
                }
                case T::H2:
                {
                    juce::AttributedString as;
                    as.setWordWrap (juce::AttributedString::byWord);
                    as.append (b.text.toUpperCase(),
                               juce::Font (fonts::label (27.0f)).withExtraKerningFactor (0.06f), colours::rust);
                    top = 22.0f;
                    hgt = single (as, 0.0f, w) + 8.0f;   // + Linie
                    after = 10.0f;
                    break;
                }
                case T::H3:
                {
                    juce::AttributedString as;
                    as.setWordWrap (juce::AttributedString::byWord);
                    as.append (b.text, juce::Font (fonts::label (20.0f)).withExtraKerningFactor (0.04f), colours::ink);
                    top = 10.0f;
                    hgt = single (as, 0.0f, w);
                    after = 4.0f;
                    break;
                }
                case T::Para:
                    hgt = single (toAttributed (b.text, bodySize, colours::ink), 0.0f, w);
                    break;

                case T::Bullet:
                case T::Numbered:
                {
                    const float ind = 26.0f + 22.0f * (float) b.indent;
                    hgt = single (toAttributed (b.text, bodySize, colours::ink), ind, w - ind);
                    after = 5.0f;
                    break;
                }
                case T::Quote:
                    hgt = single (toAttributed (b.text, bodySize, colours::ink), 18.0f, w - 30.0f) + 14.0f;
                    b.rects.back() = b.rects.back().translated (0.0f, 7.0f);
                    top = 2.0f;
                    after = 12.0f;
                    break;

                case T::Code:
                {
                    juce::AttributedString as;
                    as.setWordWrap (juce::AttributedString::byChar);
                    as.append (b.text, juce::Font (fonts::mono (13.5f, true)), colours::phosphor);
                    hgt = single (as, 14.0f, w - 28.0f) + 20.0f;
                    b.rects.back() = b.rects.back().translated (0.0f, 10.0f);
                    after = 12.0f;
                    break;
                }
                case T::Rule:
                    hgt = 18.0f;
                    break;

                case T::Table:
                {
                    const int cols = b.rows.empty() ? 0 : b.rows[0].size();
                    if (cols == 0) break;

                    // Spaltenbreiten nach natürlicher Textbreite, auf verfügbare Breite skaliert
                    std::vector<float> natural ((size_t) cols, 40.0f);
                    for (size_t r = 0; r < b.rows.size(); ++r)
                        for (int c = 0; c < cols; ++c)
                        {
                            juce::TextLayout tl;
                            tl.createLayout (toAttributed (b.rows[r][c], bodySize - 1.5f, colours::ink, r == 0), 4000.0f);
                            float lw = 0.0f;
                            for (auto& line : tl) lw = juce::jmax (lw, line.getLineBoundsX().getLength());
                            natural[(size_t) c] = juce::jmax (natural[(size_t) c], juce::jmin (lw + 18.0f, w * 0.6f));
                        }
                    float sum = 0.0f;
                    for (auto v : natural) sum += v;
                    b.colWidths.clear();
                    for (auto v : natural) b.colWidths.push_back (sum > w ? v * w / sum : v);

                    float ty = 0.0f;
                    for (size_t r = 0; r < b.rows.size(); ++r)
                    {
                        float x = 0.0f, rowH = 0.0f;
                        const size_t first = b.layouts.size();
                        for (int c = 0; c < cols; ++c)
                        {
                            const float cw = b.colWidths[(size_t) c];
                            juce::TextLayout tl;
                            tl.createLayout (toAttributed (b.rows[r][c], bodySize - 1.5f, colours::ink, r == 0), cw - 16.0f);
                            rowH = juce::jmax (rowH, tl.getHeight());
                            b.layouts.push_back (std::move (tl));
                            b.rects.push_back ({ x + 8.0f, ty + 5.0f, cw - 16.0f, 0.0f });
                            x += cw;
                        }
                        for (size_t i = first; i < b.layouts.size(); ++i)
                            b.rects[i].setHeight (rowH);
                        ty += rowH + 10.0f;
                    }
                    hgt = ty;
                    top = 2.0f;
                    after = 14.0f;
                    break;
                }
            }

            y += top;
            b.y = juce::roundToInt (y);
            b.h = juce::roundToInt (hgt);
            y += hgt + after;
        }

        return juce::roundToInt (y) + pagePad;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::cream);

        const auto clip = g.getClipBounds();
        const float x0 = (float) pagePad;
        const float w = (float) (getWidth() - 2 * pagePad);

        for (auto& bp : blocks)
        {
            auto& b = *bp;
            if (b.y > clip.getBottom() || b.y + b.h + 40 < clip.getY())
                continue;

            const float y = (float) b.y;
            using T = Block::Type;

            auto drawLayout = [&] (size_t i)
            {
                auto r = b.rects[i].translated (x0, y);
                b.layouts[i].draw (g, r.withHeight (juce::jmax (r.getHeight(), b.layouts[i].getHeight())));
            };

            switch (b.type)
            {
                case T::H2:
                    drawLayout (0);
                    g.setColour (colours::rust.withAlpha (0.6f));
                    g.fillRect (x0, y + (float) b.h - 3.0f, w, 1.5f);
                    break;

                case T::Bullet:
                case T::Numbered:
                {
                    const float ind = 26.0f + 22.0f * (float) b.indent;
                    if (b.type == T::Bullet)
                    {
                        g.setColour (colours::amber.darker (0.2f));
                        g.fillEllipse (x0 + ind - 15.0f, y + bodySize * 0.42f, 7.0f, 7.0f);
                    }
                    else
                    {
                        g.setColour (colours::rust);
                        g.setFont (fonts::mono (bodySize, true));
                        g.drawText (b.marker, juce::Rectangle<float> (x0 + ind - 30.0f, y, 26.0f, bodySize + 2.0f),
                                    juce::Justification::centredRight);
                    }
                    drawLayout (0);
                    break;
                }

                case T::Quote:
                {
                    auto r = juce::Rectangle<float> (x0, y, w, (float) b.h);
                    g.setColour (colours::amber.withAlpha (0.22f));
                    g.fillRoundedRectangle (r, 4.0f);
                    g.setColour (colours::amber.darker (0.3f));
                    g.fillRect (r.withWidth (4.0f));
                    drawLayout (0);
                    break;
                }

                case T::Code:
                    drawDisplayBezel (g, juce::Rectangle<float> (x0, y, w, (float) b.h));
                    drawLayout (0);
                    break;

                case T::Rule:
                    g.setColour (colours::engrave);
                    g.fillRect (x0, y + 8.0f, w, 1.0f);
                    break;

                case T::Table:
                {
                    if (b.colWidths.empty()) break;
                    const int cols = (int) b.colWidths.size();
                    float tw = 0.0f;
                    for (auto v : b.colWidths) tw += v;

                    for (size_t i = 0; i < b.layouts.size(); ++i)
                    {
                        const int row = (int) i / cols;
                        auto cell = b.rects[i].translated (x0, y).expanded (8.0f, 5.0f).withTrimmedBottom (0.0f);
                        if ((int) i % cols == 0)
                        {
                            const auto rowR = juce::Rectangle<float> (x0, cell.getY(), tw, cell.getHeight());
                            g.setColour (row == 0 ? colours::creamDark.darker (0.08f)
                                                  : (row % 2 == 0 ? colours::creamDark.withAlpha (0.55f) : colours::cream));
                            g.fillRect (rowR);
                            g.setColour (colours::engrave.withAlpha (0.7f));
                            g.fillRect (rowR.getX(), rowR.getBottom() - 1.0f, tw, 1.0f);
                        }
                        drawLayout (i);
                    }
                    g.setColour (colours::engrave);
                    g.drawRect (juce::Rectangle<float> (x0, y, tw, (float) b.h), 1.0f);
                    break;
                }

                case T::H1:
                case T::H3:
                case T::Para:
                    drawLayout (0);
                    break;
            }
        }
    }

private:
    std::vector<std::unique_ptr<Block>>& blocks;
};

//==============================================================================
class HandbookView::Toc : public juce::Component
{
public:
    explicit Toc (HandbookView& o) : owner (o) {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::bakelite);
        g.setColour (colours::amber.withAlpha (0.6f));
        g.fillRect (getWidth() - 2, 0, 2, getHeight());

        auto r = getLocalBounds().reduced (12, 10);
        g.setFont (juce::Font (fonts::label (16.0f)).withExtraKerningFactor (0.14f));
        g.setColour (colours::engrave);
        g.drawText (tr ("INHALT", "CONTENTS"), r.removeFromTop (22), juce::Justification::centredLeft);
        r.removeFromTop (4);

        g.setFont (fonts::mono (13.5f, true));
        for (int i = 0; i < (int) owner.chapters.size(); ++i)
        {
            auto row = r.removeFromTop (rowH);
            const bool sel = i == current, hov = i == hover;
            if (sel)
            {
                g.setColour (colours::amber.withAlpha (0.18f));
                g.fillRoundedRectangle (row.toFloat(), 3.0f);
            }
            g.setColour (sel ? colours::amberLamp : (hov ? colours::cream : colours::creamDark.withAlpha (0.8f)));
            g.drawFittedText (owner.chapters[(size_t) i].title, row.reduced (6, 0), juce::Justification::centredLeft, 2, 0.9f);
        }

        // Signatur
        auto foot = getLocalBounds().reduced (12, 10).removeFromBottom (34);
        g.setColour (colours::engrave);
        g.setFont (fonts::label (13.0f));
        g.drawText ("AME v" JucePlugin_VersionString, foot.removeFromTop (16), juce::Justification::centredLeft);
        g.setFont (fonts::mono (11.0f));
        g.drawText ("by Batze the bit builder", foot, juce::Justification::centredLeft);
    }

    int indexAt (juce::Point<int> p) const
    {
        const int i = (p.y - 10 - 26) / rowH;
        return (p.y >= 36 && i >= 0 && i < (int) owner.chapters.size()) ? i : -1;
    }

    void mouseMove (const juce::MouseEvent& e) override { const int i = indexAt (e.getPosition()); if (i != hover) { hover = i; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override   { hover = -1; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        const int i = indexAt (e.getPosition());
        if (i >= 0) owner.showChapter (i);
    }

    int current = 0, hover = -1;
    static constexpr int rowH = 30;

private:
    HandbookView& owner;
};

//==============================================================================
HandbookView::HandbookView()
{
    page = std::make_unique<Page> (blocks);
    toc = std::make_unique<Toc> (*this);
    viewport.setViewedComponent (page.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (10);
    viewport.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, colours::engrave);
    addAndMakeVisible (viewport);
    addAndMakeVisible (*toc);

    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);

    loadForCurrentLanguage();
}

HandbookView::~HandbookView() = default;

juce::String getEmbeddedManual (bool english)
{
    return english ? juce::String::fromUTF8 (BinaryData::manual_en_md, BinaryData::manual_en_mdSize)
                   : juce::String::fromUTF8 (BinaryData::handbuch_de_md, BinaryData::handbuch_de_mdSize);
}

void HandbookView::loadForCurrentLanguage()
{
    closeButton.setButtonText (tr ("SCHLIESSEN", "CLOSE"));
    loadMarkdown (getEmbeddedManual (i18n::isEnglish()));
}

void HandbookView::loadMarkdown (const juce::String& markdown)
{
    blocks.clear();
    chapters.clear();
    docTitle.clear();

    juce::StringArray lines;
    lines.addLines (markdown.replace ("\r\n", "\n"));

    using T = Block::Type;
    juce::String para;

    auto add = [this] (T type, juce::String text, int indent = 0, juce::String marker = {})
    {
        auto b = std::make_unique<Block>();
        b->type = type;
        b->text = std::move (text);
        b->indent = indent;
        b->marker = std::move (marker);
        blocks.push_back (std::move (b));
        return blocks.back().get();
    };

    auto flushPara = [&]
    {
        if (para.isNotEmpty())
            add (T::Para, para.trim());
        para.clear();
    };

    for (int i = 0; i < lines.size(); ++i)
    {
        const auto raw = lines[i];
        const auto line = raw.trimEnd();
        const auto t = line.trimStart();
        const int indent = (line.length() - t.length()) / 2;

        if (t.isEmpty())                     { flushPara(); continue; }
        if (t.startsWith ("<!--"))           { flushPara(); while (i < lines.size() && ! lines[i].contains ("-->")) ++i; continue; }
        if (t.startsWith ("!["))             { flushPara(); continue; }   // Bilder nur in der Online-Fassung

        if (t.startsWith ("```"))
        {
            flushPara();
            juce::StringArray code;
            for (++i; i < lines.size() && ! lines[i].trimStart().startsWith ("```"); ++i)
                code.add (lines[i].trimEnd());
            add (T::Code, code.joinIntoString ("\n"));
            continue;
        }

        if (t.startsWith ("### ")) { flushPara(); add (T::H3, t.substring (4).trim()); continue; }
        if (t.startsWith ("## "))
        {
            flushPara();
            const auto title = t.substring (3).trim();
            add (T::H2, title);
            chapters.push_back ({ title, (int) blocks.size() - 1 });
            continue;
        }
        if (t.startsWith ("# "))
        {
            flushPara();
            docTitle = t.substring (2).trim();
            add (T::H1, docTitle);
            continue;
        }
        if (t == "---" || t == "***") { flushPara(); add (T::Rule, {}); continue; }

        if (t.startsWithChar ('|'))
        {
            flushPara();
            auto* b = add (T::Table, {});
            for (; i < lines.size() && lines[i].trimStart().startsWithChar ('|'); ++i)
                if (! isTableSeparator (lines[i]))
                    b->rows.push_back (splitRow (lines[i]));
            --i;
            const int cols = b->rows.empty() ? 0 : b->rows[0].size();
            for (auto& r : b->rows)
                while (r.size() < cols) r.add ({});
            continue;
        }

        if (t.startsWith ("> "))
        {
            flushPara();
            juce::String q = t.substring (2);
            while (i + 1 < lines.size() && lines[i + 1].trimStart().startsWith (">"))
                q << " " << lines[++i].trimStart().substring (1).trim();
            add (T::Quote, q.trim());
            continue;
        }

        if (t.startsWith ("- ") || t.startsWith ("* "))
        {
            flushPara();
            juce::String item = t.substring (2);
            // Fortsetzungszeilen (eingerückt, kein neuer Punkt)
            while (i + 1 < lines.size())
            {
                const auto nx = lines[i + 1].trimStart();
                if (nx.isEmpty() || nx.startsWith ("- ") || nx.startsWith ("* ") || nx.startsWithChar ('|')
                    || nx.startsWithChar ('#') || ! lines[i + 1].startsWithChar (' '))
                    break;
                if (nx.initialSectionContainingOnly ("0123456789").isNotEmpty() && nx.fromFirstOccurrenceOf (".", false, false).startsWithChar (' '))
                    break;
                item << " " << nx;
                ++i;
            }
            add (T::Bullet, item.trim(), indent);
            continue;
        }

        {
            const auto num = t.initialSectionContainingOnly ("0123456789");
            if (num.isNotEmpty() && t.substring (num.length()).startsWith (". "))
            {
                flushPara();
                juce::String item = t.substring (num.length() + 2);
                while (i + 1 < lines.size() && lines[i + 1].startsWith ("   ") && ! lines[i + 1].trimStart().startsWith ("- "))
                    item << " " << lines[++i].trim();
                add (T::Numbered, item.trim(), indent, num + ".");
                continue;
            }
        }

        para << (para.isEmpty() ? "" : " ") << t;
    }
    flushPara();

    toc->current = 0;
    resized();
    repaint();
}

void HandbookView::showChapter (int index)
{
    if (index < 0 || index >= (int) chapters.size())
        return;
    toc->current = index;
    toc->repaint();
    const auto& b = *blocks[(size_t) chapters[(size_t) index].blockIndex];
    viewport.setViewPosition (0, juce::jmax (0, b.y - 14));
}

void HandbookView::resized()
{
    auto r = getLocalBounds();
    headerArea = r.removeFromTop (headerH);
    closeButton.setBounds (headerArea.reduced (10, 9).removeFromRight (120));
    tocArea = r.removeFromLeft (tocW);
    toc->setBounds (tocArea);

    viewport.setBounds (r);
    const int w = juce::jmax (200, r.getWidth() - viewport.getScrollBarThickness());
    const int h = page->layout (w);
    page->setSize (w, juce::jmax (h, r.getHeight()));
}

void HandbookView::paint (juce::Graphics& g)
{
    g.setColour (colours::bakelite);
    g.fillRect (headerArea);
    g.setColour (colours::amber.withAlpha (0.6f));
    g.fillRect (headerArea.withTop (headerArea.getBottom() - 2));

    auto h = headerArea.reduced (14, 0).toFloat();
    g.setFont (fonts::display (26.0f));
    g.setColour (colours::amberLamp);
    g.drawText ("AME", h.removeFromLeft (64.0f), juce::Justification::centredLeft);
    g.setColour (colours::cream);
    g.setFont (juce::Font (fonts::label (20.0f)).withExtraKerningFactor (0.18f));
    g.drawText (tr ("HANDBUCH", "OPERATING MANUAL"), h, juce::Justification::centredLeft);
}
} // namespace mtm::ui
