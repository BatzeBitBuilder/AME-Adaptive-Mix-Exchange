#include "PluginEditor.h"

using namespace mtm;
using mtm::i18n::tr;
namespace col = mtm::ui::colours;

MultiTrackMixEditor::MultiTrackMixEditor (MultiTrackMixProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      strip (p.apvts, p.getHub(), p.getSlot()),
      overview (p.getHub())
{
    setLookAndFeel (&lnf);

    statusLabel.setJustificationType (juce::Justification::centredRight);
    statusLabel.setColour (juce::Label::textColourId, col::cream);
    statusLabel.setFont (mtm::ui::fonts::mono (13.0f, true));
    statusLabel.setMinimumHorizontalScale (0.75f);
    addAndMakeVisible (statusLabel);

    auto setupCombo = [this] (juce::ComboBox& box, juce::Label& label, const juce::String& text,
                              const juce::StringArray& items)
    {
        label.setText (text, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, col::cream);
        label.setFont (mtm::ui::fonts::label (17.0f));
        label.setJustificationType (juce::Justification::centredRight);
        box.addItemList (items, 1);
        addAndMakeVisible (label);
        addAndMakeVisible (box);
    };

    setupCombo (modeBox,  modeLabel,  {}, getModeNames());
    setupCombo (roleBox,  roleLabel,  {}, getRoleNames());
    setupCombo (genreBox, genreLabel, {}, getGenreNames());

    modeAtt  = std::make_unique<ComboAttachment> (p.apvts, pid::mode,  modeBox);
    roleAtt  = std::make_unique<ComboAttachment> (p.apvts, pid::role,  roleBox);
    genreAtt = std::make_unique<ComboAttachment> (p.apvts, pid::genre, genreBox);

    autoRoleLabel.setColour (juce::Label::textColourId, col::amberLamp);
    autoRoleLabel.setFont (mtm::ui::fonts::mono (14.0f, true));
    addAndMakeVisible (autoRoleLabel);

    globalAmountLabel.setText ("Amount", juce::dontSendNotification);
    globalAmountLabel.setColour (juce::Label::textColourId, col::cream);
    globalAmountLabel.setFont (mtm::ui::fonts::label (17.0f));
    globalAmount.setColour (juce::Slider::textBoxTextColourId, col::amberLamp);
    globalBypass.setColour (juce::ToggleButton::textColourId, col::cream);
    globalAmountLabel.setJustificationType (juce::Justification::centredRight);
    globalAmount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
    addAndMakeVisible (globalAmountLabel);
    addAndMakeVisible (globalAmount);
    globalAmountAtt = std::make_unique<SliderAttachment> (p.apvts, pid::globalAmount, globalAmount);

    addAndMakeVisible (globalBypass);
    globalBypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, pid::globalBypass, globalBypass);
    freezeAllButton.onClick  = [this] { proc.getHub().sendFreezeCommand (true); };
    releaseAllButton.onClick = [this] { proc.getHub().sendFreezeCommand (false); };
    addAndMakeVisible (freezeAllButton);
    addAndMakeVisible (releaseAllButton);

    overviewButton.setClickingTogglesState (true);
    overviewButton.onClick = [this] { updateVisibility(); };
    addAndMakeVisible (overviewButton);

    // Sprache (global für alle Instanzen) + Handbuch
    for (auto* b : { &langDe, &langEn })
    {
        b->setClickingTogglesState (false);
        b->setConnectedEdges (b == &langDe ? juce::Button::ConnectedOnRight : juce::Button::ConnectedOnLeft);
        addAndMakeVisible (*b);
    }
    langDe.onClick = [] { i18n::setLanguage (i18n::Language::German); };
    langEn.onClick = [] { i18n::setLanguage (i18n::Language::English); };
    manualButton.setClickingTogglesState (true);
    manualButton.onClick = [this] { showHandbook (manualButton.getToggleState()); };
    addAndMakeVisible (manualButton);

    addChildComponent (strip);
    addChildComponent (overview);
    addChildComponent (handbook);
    handbook.onClose = [this] { showHandbook (false); };

    updateTexts();
    i18n::addListener (this);

    lastMode = proc.getMode();
    updateVisibility();
    sendLookAndFeelChange();   // Kinder, die nach setLookAndFeel hinzugefügt wurden

    setSize (1000, 880);
    startTimerHz (30);
}

MultiTrackMixEditor::~MultiTrackMixEditor()
{
    i18n::removeListener (this);
    stopTimer();
    setLookAndFeel (nullptr);
}

void MultiTrackMixEditor::refillCombo (juce::ComboBox& box, const juce::StringArray& items)
{
    const int sel = box.getSelectedItemIndex();
    box.clear (juce::dontSendNotification);
    box.addItemList (items, 1);
    box.setSelectedItemIndex (sel, juce::dontSendNotification);
}

void MultiTrackMixEditor::updateTexts()
{
    modeLabel.setText (tr ("Modus", "Mode"), juce::dontSendNotification);
    roleLabel.setText (tr ("Rolle", "Role"), juce::dontSendNotification);
    genreLabel.setText (tr ("Programm", "Program"), juce::dontSendNotification);
    refillCombo (modeBox, getModeNames());
    refillCombo (roleBox, getRoleNames());
    refillCombo (genreBox, getGenreNames());

    overviewButton.setButtonText (tr ("Mix-Übersicht", "Mix Overview"));
    globalBypass.setButtonText (tr ("Direkt (alle)", "Direct (all)"));
    freezeAllButton.setButtonText (tr ("Hold (alle)", "Hold (all)"));
    releaseAllButton.setButtonText (tr ("Hold lösen", "Release"));
    manualButton.setButtonText (tr ("HANDBUCH", "MANUAL"));
    manualButton.setTooltip (tr ("Handbuch öffnen", "Open the manual"));

    const bool en = i18n::isEnglish();
    langDe.setToggleState (! en, juce::dontSendNotification);
    langEn.setToggleState (en, juce::dontSendNotification);
    langDe.setTooltip ("Deutsch");
    langEn.setTooltip ("English");

    strip.updateTexts();
    overview.updateTexts();
    handbook.loadForCurrentLanguage();
    frame = 2;   // Statuszeile beim nächsten Tick neu
    repaint();
}

void MultiTrackMixEditor::languageChanged()
{
    updateTexts();
}

void MultiTrackMixEditor::showHandbook (bool shouldShow)
{
    manualButton.setToggleState (shouldShow, juce::dontSendNotification);
    handbook.setVisible (shouldShow);
    if (shouldShow)
        handbook.toFront (false);
}

void MultiTrackMixEditor::updateVisibility()
{
    const bool master = proc.getMode() == InstanceMode::Master;

    roleLabel.setVisible (! master);
    roleBox.setVisible (! master);
    autoRoleLabel.setVisible (! master);
    overviewButton.setVisible (! master);

    genreLabel.setVisible (master);
    genreBox.setVisible (master);
    globalAmountLabel.setVisible (master);
    globalAmount.setVisible (master);
    globalBypass.setVisible (master);
    freezeAllButton.setVisible (master);
    releaseAllButton.setVisible (master);

    const bool showOverview = master || overviewButton.getToggleState();
    overview.setVisible (showOverview);
    strip.setVisible (! showOverview);
}

void MultiTrackMixEditor::timerCallback()
{
    const auto mode = proc.getMode();
    if (mode != lastMode)
    {
        lastMode = mode;
        updateVisibility();
        resized();
    }

    if (strip.isVisible())
        strip.refreshMeters();

    if (overview.isVisible())
        overview.refresh (proc.getInstanceId(), 1.0f / 30.0f);

    // Erkannte Rolle bei "Auto"
    if (mode == InstanceMode::Track && (Role) roleBox.getSelectedItemIndex() == Role::Auto)
    {
        const auto g = proc.getAutoRole();
        juce::String t;
        if (g.role == Role::Auto)
            t = tr ("→ analysiere …", "→ analysing …");
        else
            t = MTM_TXT ("→ ") + getRoleNames()[(int) g.role]
              + (g.fromName ? tr ("  (Spurname)", "  (track name)")
                            : "  (" + juce::String (juce::roundToInt (g.confidence * 100.0f)) + " %)");
        autoRoleLabel.setText (t, juce::dontSendNotification);
    }
    else
    {
        autoRoleLabel.setText ({}, juce::dontSendNotification);
    }

    // Übersicht + Status mit 10 Hz
    if (++frame % 3 != 0)
        return;


    auto& hub = proc.getHub();
    const int n = hub.getNumInstances();
    const int masters = hub.getNumMasters();

    juce::String status = juce::String (n) + (n == 1 ? " Line" : " Lines");
    juce::Colour c = col::cream;

    if (masters == 0)
    {
        status << tr ("  ·  No Exchange – Line läuft solo (AME auf den Master-Bus, Modus \"Exchange\")",
                      "  ·  No Exchange – line runs solo (put AME on the master bus, mode \"Exchange\")");
        c = col::amberLamp;
    }
    else if (masters > 1)
    {
        status << MTM_TXT ("  ·  ") << masters << tr (" Exchanges – nur das älteste ist aktiv", " exchanges – only the oldest is active");
        c = col::amberLamp;
    }
    else if (mode == InstanceMode::Master)
    {
        status << tr ("  ·  Exchange aktiv", "  ·  Exchange active");
    }

    statusLabel.setText (status, juce::dontSendNotification);
    statusLabel.setTooltip (status);

    // Lampen am Schriftzug: Line verbunden / Engine aktiv / Hold
    const bool line = masters > 0;
    const bool engine = hub.isEngineAlive();
    const bool hold = mode == InstanceMode::Track && proc.apvts.getRawParameterValue (pid::adaptFreeze)->load() > 0.5f;
    if (line != lampLine || engine != lampEngine || hold != lampHold)
    {
        lampLine = line; lampEngine = engine; lampHold = hold;
        repaint (logoArea);
    }
    statusLabel.setColour (juce::Label::textColourId, c);
}

void MultiTrackMixEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    // Walnuss-Gehäuse mit Maserung
    g.fillAll (col::walnut);
    {
        juce::Random rnd (42);
        for (int i = 0; i < 140; ++i)
        {
            const float y0 = rnd.nextFloat() * b.getHeight();
            const float amp = 2.0f + rnd.nextFloat() * 6.0f, freq = 0.004f + rnd.nextFloat() * 0.01f, ph = rnd.nextFloat() * 6.28f;
            juce::Path grain;
            const int steps = (int) (b.getWidth() / 8.0f);
            for (int s = 0; s <= steps; ++s)
            {
                const float x = (float) s * 8.0f;
                const float y = y0 + amp * std::sin (x * freq + ph) + 0.6f * amp * std::sin (x * freq * 2.7f + ph * 1.3f);
                if (s == 0) grain.startNewSubPath (x, y); else grain.lineTo (x, y);
            }
            g.setColour ((rnd.nextBool() ? col::walnutLight : juce::Colour (0xff2e1a0e)).withAlpha (0.10f + rnd.nextFloat() * 0.15f));
            g.strokePath (grain, juce::PathStrokeType (0.6f + rnd.nextFloat() * 1.4f));
        }
    }

    // Kopfleiste (Bakelit) und Front (Creme)
    auto header = b.withTrimmedLeft (cheek).withTrimmedRight (cheek).withHeight (headerH);
    g.setColour (col::bakelite);
    g.fillRect (header);
    g.setColour (col::amber.withAlpha (0.6f));
    g.fillRect (header.withTop (header.getBottom() - 2.0f));

    auto front = b.withTrimmedLeft (cheek).withTrimmedRight (cheek).withTrimmedTop (headerH);
    g.setColour (col::cream);
    g.fillRect (front);
    {
        // feine Struktur der Lackierung
        juce::Random rnd (7);
        for (int i = 0; i < 1600; ++i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.025f + rnd.nextFloat() * 0.03f));
            g.fillRect (front.getX() + rnd.nextFloat() * front.getWidth(), front.getY() + rnd.nextFloat() * front.getHeight(), 1.5f, 1.5f);
        }
    }

    paintLogo (g);

    // Schrauben
    for (auto p : { front.getTopLeft().translated (7.0f, 7.0f), front.getTopRight().translated (-7.0f, 7.0f),
                    front.getBottomLeft().translated (7.0f, -7.0f), front.getBottomRight().translated (-7.0f, -7.0f) })
        mtm::ui::drawScrew (g, p, 4.0f);
}

void MultiTrackMixEditor::resized()
{
    auto area = getLocalBounds().withTrimmedLeft (cheek).withTrimmedRight (cheek);
    auto header = area.removeFromTop (headerH).reduced (14, 8);
    area.reduce (8, 6);

    auto line1 = header.removeFromTop (30);
    logoArea = line1.removeFromLeft (270);

    // rechts außen: Handbuch + Sprache
    {
        auto r = line1;
        manualButton.setBounds (r.removeFromRight (84).reduced (0, 1));
        r.removeFromRight (6);
        langEn.setBounds (r.removeFromRight (32).reduced (0, 1));
        langDe.setBounds (r.removeFromRight (32).reduced (0, 1));
        r.removeFromRight (12);
        line1 = r;
    }
    // Master: Bypass / Einfrieren rechts in Zeile 1
    if (proc.getMode() == InstanceMode::Master)
    {
        releaseAllButton.setBounds (line1.removeFromRight (86).reduced (0, 1));
        line1.removeFromRight (6);
        freezeAllButton.setBounds (line1.removeFromRight (100).reduced (0, 1));
        line1.removeFromRight (10);
        globalBypass.setBounds (line1.removeFromRight (124));
        line1.removeFromRight (8);
    }
    statusLabel.setBounds (line1);

    header.removeFromTop (6);
    auto line2 = header.removeFromTop (26);

    modeLabel.setBounds (line2.removeFromLeft (52));
    line2.removeFromLeft (6);
    modeBox.setBounds (line2.removeFromLeft (150));
    line2.removeFromLeft (16);

    // Spur-Modus
    {
        auto r = line2;
        roleLabel.setBounds (r.removeFromLeft (50));
        r.removeFromLeft (6);
        roleBox.setBounds (r.removeFromLeft (170));
        r.removeFromLeft (8);
        autoRoleLabel.setBounds (r.removeFromLeft (260));
        overviewButton.setBounds (r.removeFromRight (130));
    }
    // Master-Modus
    {
        auto r = line2;
        genreLabel.setBounds (r.removeFromLeft (76));
        r.removeFromLeft (6);
        genreBox.setBounds (r.removeFromLeft (220));
        r.removeFromLeft (16);
        globalAmountLabel.setBounds (r.removeFromLeft (60));
        r.removeFromLeft (6);
        globalAmount.setBounds (r.removeFromLeft (220));
    }

    strip.setBounds (area);
    overview.setBounds (area);
    handbook.setBounds (getLocalBounds().withTrimmedLeft (cheek).withTrimmedRight (cheek).withTrimmedTop (headerH));
}

void MultiTrackMixEditor::paintLogo (juce::Graphics& g)
{
    auto la = logoArea.toFloat();
    if (la.isEmpty())
        return;

    // "AME" in Bernstein mit leichtem Glühen
    auto word = la.removeFromLeft (84.0f).withTrimmedTop (-6.0f);
    g.setFont (mtm::ui::fonts::display (36.0f));
    g.setColour (col::amber.withAlpha (0.25f));
    for (auto d : { -1.0f, 1.0f })
        g.drawText ("AME", word.translated (d, 0.0f), juce::Justification::centredLeft);
    g.setColour (col::amberLamp);
    g.drawText ("AME", word, juce::Justification::centredLeft);

    // Untertitel, gesperrt
    la.removeFromLeft (6.0f);
    auto sub = la.removeFromTop (14.0f);
    g.setColour (col::cream);
    g.setFont (juce::Font (mtm::ui::fonts::label (14.0f)).withExtraKerningFactor (0.18f));
    g.drawText ("ADAPTIVE MIX EXCHANGE", sub, juce::Justification::centredLeft);

    // Klinken-Lampen
    la.removeFromTop (3.0f);
    struct Lamp { const char* text; bool on; juce::Colour c; };
    const Lamp lamps[] { { "LINE", lampLine, col::phosphor },
                         { "ENGINE", lampEngine, col::amberLamp },
                         { "HOLD", lampHold, juce::Colour (0xff8fd0ff) } };
    float x = la.getX();
    const float cy = la.getY() + 6.0f;
    g.setFont (mtm::ui::fonts::label (13.0f));
    for (const auto& l : lamps)
    {
        const juce::Rectangle<float> r (x, cy - 4.0f, 8.0f, 8.0f);
        if (l.on)
        {
            g.setColour (l.c.withAlpha (0.35f));
            g.fillEllipse (r.expanded (2.5f));
        }
        g.setColour (l.on ? l.c : juce::Colour (0xff3a3128));
        g.fillEllipse (r);
        g.setColour (col::engrave);
        g.drawEllipse (r, 1.0f);
        g.setColour (l.on ? col::cream : col::engrave);
        const float w = l.text[0] == 'E' ? 40.0f : 30.0f;
        g.drawText (l.text, juce::Rectangle<float> (x + 11.0f, cy - 6.0f, w, 12.0f), juce::Justification::centredLeft);
        x += 11.0f + w + 4.0f;
    }

    g.setColour (col::engrave);
    g.setFont (mtm::ui::fonts::mono (10.0f, true));
    g.drawText ("v" JucePlugin_VersionString, juce::Rectangle<float> (x + 2.0f, cy - 6.0f, la.getRight() - x - 2.0f, 12.0f),
                juce::Justification::centredLeft);
}
