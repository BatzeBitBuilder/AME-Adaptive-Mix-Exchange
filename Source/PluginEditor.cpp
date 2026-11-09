// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "PluginEditor.h"
#include "Settings.h"

using namespace ame;
using ame::i18n::tr;
namespace col = ame::ui::colours;

MainView::MainView (AmeProcessor& p)
    : proc (p),
      strip (p.apvts, p.getHub(), p.getSlot()),
      overview (p.getHub())
{
    setLookAndFeel (&lnf);

    statusLabel.setJustificationType (juce::Justification::centredRight);
    statusLabel.setColour (juce::Label::textColourId, col::cream);
    statusLabel.setFont (ame::ui::fonts::mono (13.0f, true));
    statusLabel.setMinimumHorizontalScale (0.75f);
    addAndMakeVisible (statusLabel);

    auto setupCombo = [this] (juce::ComboBox& box, juce::Label& label, const juce::String& text,
                              const juce::StringArray& items)
    {
        label.setText (text, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, col::cream);
        label.setFont (ame::ui::fonts::label (17.0f));
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
    autoRoleLabel.setFont (ame::ui::fonts::mono (14.0f, true));
    addAndMakeVisible (autoRoleLabel);

    globalAmountLabel.setText ("Amount", juce::dontSendNotification);
    globalAmountLabel.setColour (juce::Label::textColourId, col::cream);
    globalAmountLabel.setFont (ame::ui::fonts::label (17.0f));
    globalAmount.setColour (juce::Slider::textBoxTextColourId, col::amberLamp);
    globalBypass.setColour (juce::ToggleButton::textColourId, col::cream);
    globalAmountLabel.setJustificationType (juce::Justification::centredRight);
    globalAmount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
    addAndMakeVisible (globalAmountLabel);
    addAndMakeVisible (globalAmount);
    globalAmountAtt = std::make_unique<SliderAttachment> (p.apvts, pid::globalAmount, globalAmount);

    globalBypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, pid::globalBypass, globalBypass);

    // Abhöre: Voll / Ohne Amt / Roh
    monitorLabel.setFont (ame::ui::fonts::label (17.0f));
    monitorLabel.setColour (juce::Label::textColourId, col::cream);
    monitorLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (monitorLabel);
    int mi = 0;
    for (auto* b : { &monitorFull, &monitorNoEngine, &monitorRaw })
    {
        b->setClickingTogglesState (false);
        b->onClick = [this, mi] { proc.setMonitor (mi); };
        addAndMakeVisible (*b);
        ++mi;
    }
    monitorFull.setConnectedEdges (juce::Button::ConnectedOnRight);
    monitorNoEngine.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    monitorRaw.setConnectedEdges (juce::Button::ConnectedOnLeft);
    levelMatch.setColour (juce::ToggleButton::textColourId, col::cream);
    addAndMakeVisible (levelMatch);
    levelMatchAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, pid::levelMatch, levelMatch);
    freezeAllButton.onClick  = [this] { proc.getHub().sendFreezeCommand (true); };
    releaseAllButton.onClick = [this] { proc.getHub().sendFreezeCommand (false); };
    addAndMakeVisible (freezeAllButton);
    addAndMakeVisible (releaseAllButton);

    // Mitschnitt (alle Lines)
    learnBarLabel.setFont (ame::ui::fonts::label (17.0f));
    learnBarLabel.setColour (juce::Label::textColourId, col::ink);
    addAndMakeVisible (learnBarLabel);
    learnStatus.setFont (ame::ui::fonts::mono (12.0f, true));
    learnStatus.setColour (juce::Label::textColourId, col::ink);
    learnStatus.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (learnStatus);
    learnRecAll.onClick   = [this] { proc.getHub().sendLearnCommand (MixHub::learnRecord); };
    learnPlayAll.onClick  = [this] { proc.getHub().sendLearnCommand (MixHub::learnPlay); };
    learnLiveAll.onClick  = [this] { proc.getHub().sendLearnCommand (MixHub::learnLive); };
    learnClearAll.onClick = [this]
    {
        juce::NativeMessageBox::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon,
            tr ("Mitschnitt löschen", "Clear recording"),
            tr ("Die Mitschnitte aller Lines löschen?", "Clear the recordings of all lines?"), this,
            juce::ModalCallbackFunction::create ([this] (int r)
            {
                if (r != 0)
                    proc.getHub().sendLearnCommand (MixHub::learnClear);
            }));
    };
    for (auto* b : { &learnRecAll, &learnPlayAll, &learnLiveAll, &learnClearAll })
        addAndMakeVisible (*b);

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

    // Fenstergröße per Menü (funktioniert auch, wenn die Zieh-Ecke außerhalb des Bildschirms liegt)
    zoomButton.onClick = [this]
    {
        juce::PopupMenu m;
        m.addSectionHeader (tr ("Fenstergröße", "Window size"));
        const int steps[] { 50, 60, 70, 80, 90, 100, 110, 125, 150, 175, 200 };
        const int cur = juce::roundToInt (shownScale * 100.0f);
        for (int pc : steps)
            m.addItem (pc, juce::String (pc) + " %", true, std::abs (pc - cur) <= 2);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&zoomButton),
                         [safe = juce::Component::SafePointer<MainView> (this)] (int r)
                         {
                             if (safe != nullptr && r > 0 && safe->onScaleChosen)
                                 safe->onScaleChosen ((float) r / 100.0f);
                         });
    };
    addAndMakeVisible (zoomButton);

    addChildComponent (strip);
    addChildComponent (overview);
    addChildComponent (handbook);
    handbook.onClose = [this] { showHandbook (false); };

    updateTexts();
    i18n::addListener (this);

    lastMode = proc.getMode();
    updateVisibility();
    sendLookAndFeelChange();   // Kinder, die nach setLookAndFeel hinzugefügt wurden

    setSize (baseWidth, baseHeight);
    startTimerHz (30);
}

MainView::~MainView()
{
    i18n::removeListener (this);
    stopTimer();
    setLookAndFeel (nullptr);
}

void MainView::refillCombo (juce::ComboBox& box, const juce::StringArray& items)
{
    const int sel = box.getSelectedItemIndex();
    box.clear (juce::dontSendNotification);
    box.addItemList (items, 1);
    box.setSelectedItemIndex (sel, juce::dontSendNotification);
}

void MainView::updateTexts()
{
    modeLabel.setText (tr ("Modus", "Mode"), juce::dontSendNotification);
    roleLabel.setText (tr ("Rolle", "Role"), juce::dontSendNotification);
    genreLabel.setText (tr ("Programm", "Program"), juce::dontSendNotification);
    refillCombo (modeBox, getModeNames());
    refillCombo (roleBox, getRoleNames());
    refillCombo (genreBox, getGenreNames());

    overviewButton.setButtonText (tr ("Mix-Übersicht", "Mix Overview"));
    globalBypass.setButtonText (tr ("Direkt (alle)", "Direct (all)"));
    monitorLabel.setText (tr ("Abhöre", "Monitor"), juce::dontSendNotification);
    monitorFull.setButtonText (tr ("Voll", "Full"));
    monitorNoEngine.setButtonText (tr ("Ohne Amt", "No Engine"));
    monitorRaw.setButtonText (tr ("Roh", "Raw"));
    levelMatch.setButtonText (tr ("Pegelangleich", "Level match"));
    monitorFull.setTooltip (tr ("Alles aktiv: Channelstrips + Engine", "Everything active: channel strips + engine"));
    monitorNoEngine.setTooltip (tr ("Nur die manuellen Channelstrips, Engine-Eingriffe aus", "Manual channel strips only, engine processing off"));
    monitorRaw.setTooltip (tr ("Alle Lines unbearbeitet – wie ohne Plugins (wird nicht im Projekt gespeichert)",
                               "All lines unprocessed – as without plug-ins (not saved with the project)"));
    levelMatch.setTooltip (tr ("Roh: Lautheitsunterschied zur bearbeiteten Fassung am Exchange-Ausgang ausgleichen – fairer Vergleich",
                               "Raw: compensate the loudness difference to the processed mix at the exchange output – fair comparison"));
    freezeAllButton.setButtonText (tr ("Hold (alle)", "Hold (all)"));
    learnBarLabel.setText (tr ("MITSCHNITT", "RECORDING"), juce::dontSendNotification);
    learnRecAll.setButtonText (tr ("Aufnahme (alle)", "Record (all)"));
    learnPlayAll.setButtonText (tr ("Wiedergabe (alle)", "Playback (all)"));
    learnLiveAll.setButtonText (tr ("Live (alle)", "Live (all)"));
    learnClearAll.setButtonText (tr ("Löschen", "Clear"));
    learnRecAll.setTooltip (tr ("Alle Lines nehmen ihre adaptiven Werte über die Songposition auf, solange der Transport läuft. "
                                "Song einmal komplett abspielen (Abhöre Voll).",
                                "All lines record their adaptive values along the song position while the transport runs. "
                                "Play the whole song once (monitor Full)."));
    learnPlayAll.setTooltip (tr ("Alle Lines spielen ihren Mitschnitt ab: Export = Abhöre, unabhängig von Engine und Render-Geschwindigkeit.",
                                 "All lines play back their recording: export = monitor, independent of engine and render speed."));
    learnLiveAll.setTooltip (tr ("Alle Lines wieder live: die Engine regelt laufend (Mitschnitt bleibt erhalten).",
                                 "All lines back to live: the engine adapts continuously (recording is kept)."));
    learnClearAll.setTooltip (tr ("Mitschnitte aller Lines löschen", "Clear the recordings of all lines"));
    releaseAllButton.setButtonText (tr ("Hold lösen", "Release"));
    manualButton.setButtonText (tr ("HANDBUCH", "MANUAL"));
    manualButton.setTooltip (tr ("Handbuch öffnen", "Open the manual"));
    zoomButton.setTooltip (tr ("Fenstergröße wählen (gilt für alle AME-Fenster)", "Choose the window size (applies to all AME windows)"));
    setScaleDisplay (shownScale);

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

void MainView::setScaleDisplay (float s)
{
    shownScale = s;
    zoomButton.setButtonText (juce::String (juce::roundToInt (s * 100.0f)) + " %");
}

void MainView::languageChanged()
{
    updateTexts();
}

void MainView::showHandbook (bool shouldShow)
{
    manualButton.setToggleState (shouldShow, juce::dontSendNotification);
    handbook.setVisible (shouldShow);
    if (shouldShow)
        handbook.toFront (false);
}

void MainView::updateVisibility()
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
    globalBypass.setVisible (false);
    for (juce::Component* c : { (juce::Component*) &monitorLabel, (juce::Component*) &monitorFull, (juce::Component*) &monitorNoEngine,
                                (juce::Component*) &monitorRaw, (juce::Component*) &levelMatch })
        c->setVisible (master);
    freezeAllButton.setVisible (master);
    releaseAllButton.setVisible (master);
    for (juce::Component* c : { (juce::Component*) &learnBarLabel, (juce::Component*) &learnStatus, (juce::Component*) &learnRecAll,
                                (juce::Component*) &learnPlayAll, (juce::Component*) &learnLiveAll, (juce::Component*) &learnClearAll })
        c->setVisible (master);

    const bool showOverview = master || overviewButton.getToggleState();
    overview.setVisible (showOverview);
    strip.setVisible (! showOverview);
}

void MainView::timerCallback()
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
            t = AME_TXT ("→ ") + getRoleNames()[(int) g.role]
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
        status << AME_TXT ("  ·  ") << masters << tr (" Exchanges – nur das älteste ist aktiv", " exchanges – only the oldest is active");
        c = col::amberLamp;
    }
    else if (! hub.isEngineAlive())
    {
        status << tr ("  ·  Exchange deaktiviert – Engine aus", "  ·  Exchange deactivated – engine off");
        c = col::amberLamp;
    }
    else if (hub.isRawMonitor())
    {
        status << tr ("  ·  Abhöre ROH – alle Lines unbearbeitet", "  ·  Monitor RAW – all lines unprocessed");
        if (mode == InstanceMode::Master && proc.apvts.getRawParameterValue (pid::levelMatch)->load() > 0.5f)
            status << tr ("  ·  Pegelangleich ", "  ·  level match ") << juce::String (hub.rawMatchDb.load(), 1) << " dB";
        c = juce::Colour (0xffff7a5c);
    }
    else if (hub.monitorMode.load() == 1)
    {
        status << tr ("  ·  Abhöre OHNE AMT – Engine-Eingriffe aus", "  ·  Monitor NO ENGINE – engine processing off");
        c = col::amberLamp;
    }
    else if (mode == InstanceMode::Master)
    {
        status << tr ("  ·  Exchange aktiv", "  ·  Exchange active");
    }

    // Abhöre-Tasten folgen dem Parameter (auch bei Automation / MIDI)
    {
        const int mon = juce::roundToInt (proc.apvts.getRawParameterValue (pid::monitor)->load());
        monitorFull.setToggleState (mon == 0, juce::dontSendNotification);
        monitorNoEngine.setToggleState (mon == 1, juce::dontSendNotification);
        monitorRaw.setToggleState (mon == 2, juce::dontSendNotification);
    }

    statusLabel.setText (status, juce::dontSendNotification);

    // Mitschnitt-Übersicht (Exchange)
    if (mode == InstanceMode::Master)
    {
        int live = 0, rec = 0, play = 0, warn = 0, recNow = 0;
        float longest = 0.0f;
        const auto nowMs = juce::Time::getMillisecondCounter();
        for (const auto& t : hub.getSnapshot())
            if (t.mode == InstanceMode::Track)
                if (auto* d = hub.getAudioData (t.slot))
                {
                    const int lm = d->learnMode.load();
                    (lm == 1 ? rec : lm == 2 ? play : live)++;
                    if (d->learnActive.load()) ++recNow;
                    longest = juce::jmax (longest, d->learnSec.load());
                    const auto rw = d->renderWarnMs.load();
                    if (rw != 0 && nowMs - rw < 5u * 60u * 1000u) ++warn;
                }
        const int sec = juce::roundToInt (longest);
        juce::String ls;
        ls << "LIVE " << live << tr (" · AUFNAHME ", " · RECORD ") << rec << tr (" · WIEDERGABE ", " · PLAYBACK ") << play
           << AME_TXT (" · ") << (sec / 60) << ":" << juce::String (sec % 60).paddedLeft ('0', 2);
        juce::Colour lc = col::ink;
        if (recNow > 0)
        {
            ls << tr (" · NIMMT AUF", " · RECORDING");
            lc = col::warn;
        }
        if (warn > 0)
        {
            ls << AME_TXT (" · ") << warn << tr ("× EXPORT OHNE EXCHANGE", "× EXPORT WITHOUT EXCHANGE");
            lc = col::rust;
        }
        learnStatus.setText (ls, juce::dontSendNotification);
        learnStatus.setColour (juce::Label::textColourId, lc);
    }

    // Tooltip: Art der Verbindung zwischen den Instanzen
    const int procs = hub.getNumProcesses();
    juce::String link = hub.isShared()
        ? tr ("Verbindung: Shared Memory, DAW-Sitzung ", "Link: shared memory, DAW session ") + hub.getSessionKey()
              + (procs > 1 ? juce::String (AME_TXT (" · ")) + juce::String (procs) + tr (" Prozesse", " processes") : juce::String())
        : tr ("Verbindung: nur innerhalb dieses Prozesses", "Link: this process only");
    statusLabel.setTooltip (status + "\n" + link);

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

void MainView::paint (juce::Graphics& g)
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

    if (! learnBar.isEmpty())
        ame::ui::drawEngravedPanel (g, learnBar.toFloat(), {});

    // Schrauben
    for (auto p : { front.getTopLeft().translated (7.0f, 7.0f), front.getTopRight().translated (-7.0f, 7.0f),
                    front.getBottomLeft().translated (7.0f, -7.0f), front.getBottomRight().translated (-7.0f, -7.0f) })
        ame::ui::drawScrew (g, p, 4.0f);
}

void MainView::resized()
{
    auto area = getLocalBounds().withTrimmedLeft (cheek).withTrimmedRight (cheek);
    auto header = area.removeFromTop (headerH).reduced (14, 8);
    area.reduce (8, 6);

    auto line1 = header.removeFromTop (30);
    logoArea = line1.removeFromLeft (296);

    // rechts außen: Handbuch + Sprache
    {
        auto r = line1;
        manualButton.setBounds (r.removeFromRight (84).reduced (0, 1));
        r.removeFromRight (6);
        zoomButton.setBounds (r.removeFromRight (62).reduced (0, 1));
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
        levelMatch.setBounds (line1.removeFromRight (128));
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
        globalAmount.setBounds (r.removeFromLeft (160));

        monitorRaw.setBounds (r.removeFromRight (58).reduced (0, 1));
        monitorNoEngine.setBounds (r.removeFromRight (86).reduced (0, 1));
        monitorFull.setBounds (r.removeFromRight (58).reduced (0, 1));
        r.removeFromRight (6);
        monitorLabel.setBounds (r.removeFromRight (64));
    }

    strip.setBounds (area);

    // Exchange: Mitschnitt-Leiste über der Mix-Übersicht
    learnBar = {};
    if (proc.getMode() == InstanceMode::Master)
    {
        learnBar = area.removeFromTop (44).reduced (8, 0).withTrimmedTop (6);
        auto bar = learnBar.reduced (10, 4);
        learnBarLabel.setBounds (bar.removeFromLeft (104));
        learnRecAll.setBounds (bar.removeFromLeft (132).reduced (0, 2));
        bar.removeFromLeft (4);
        learnPlayAll.setBounds (bar.removeFromLeft (142).reduced (0, 2));
        bar.removeFromLeft (4);
        learnLiveAll.setBounds (bar.removeFromLeft (100).reduced (0, 2));
        bar.removeFromLeft (4);
        learnClearAll.setBounds (bar.removeFromLeft (80).reduced (0, 2));
        bar.removeFromLeft (12);
        learnStatus.setBounds (bar);
    }
    overview.setBounds (area);
    handbook.setBounds (getLocalBounds().withTrimmedLeft (cheek).withTrimmedRight (cheek).withTrimmedTop (headerH));
}

void MainView::paintLogo (juce::Graphics& g)
{
    auto la = logoArea.toFloat();
    if (la.isEmpty())
        return;

    // "AME" in Bernstein mit leichtem Glühen
    auto word = la.removeFromLeft (84.0f).withTrimmedTop (-6.0f);
    g.setFont (ame::ui::fonts::display (36.0f));
    g.setColour (col::amber.withAlpha (0.25f));
    for (auto d : { -1.0f, 1.0f })
        g.drawText ("AME", word.translated (d, 0.0f), juce::Justification::centredLeft);
    g.setColour (col::amberLamp);
    g.drawText ("AME", word, juce::Justification::centredLeft);

    // Untertitel, gesperrt
    la.removeFromLeft (6.0f);
    auto sub = la.removeFromTop (14.0f);
    g.setColour (col::cream);
    g.setFont (juce::Font (ame::ui::fonts::label (14.0f)).withExtraKerningFactor (0.18f));
    g.drawText ("ADAPTIVE MIX EXCHANGE", sub, juce::Justification::centredLeft);

    // Klinken-Lampen
    la.removeFromTop (3.0f);
    struct Lamp { const char* text; bool on; juce::Colour c; };
    const Lamp lamps[] { { "LINE", lampLine, col::phosphor },
                         { "ENGINE", lampEngine, col::amberLamp },
                         { "HOLD", lampHold, juce::Colour (0xff8fd0ff) } };
    float x = la.getX();
    const float cy = la.getY() + 6.0f;
    g.setFont (ame::ui::fonts::label (13.0f));
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
    g.setFont (ame::ui::fonts::mono (10.0f, true));
    g.drawText ("v" JucePlugin_VersionString, juce::Rectangle<float> (x + 2.0f, cy - 6.0f, la.getRight() - x - 2.0f, 12.0f),
                juce::Justification::centredLeft);
}

//==============================================================================
AmeEditor::AmeEditor (AmeProcessor& p)
    : AudioProcessorEditor (&p), view (p)
{
    addAndMakeVisible (view);

    // frei skalierbar mit festem Seitenverhältnis; zuletzt genutzte Größe gilt für alle Instanzen
    constexpr int w = MainView::baseWidth, h = MainView::baseHeight;
    setResizable (true, true);
    setResizeLimits (w / 2, h / 2, w * 2, h * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) w / (double) h);

    view.onScaleChosen = [this] (float s) { setUiScale (s); };

    const float stored = ame::settings::get ("uiScale", "0.9").getFloatValue();
    const float s = juce::jlimit (0.5f, 2.0f, stored > 0.0f ? stored : 0.9f);
    setSize (juce::roundToInt ((float) w * s), juce::roundToInt ((float) h * s));

    // Monitor prüfen (beim Öffnen und nach dem Verschieben auf einen kleineren Bildschirm)
    startTimer (500);
}

void AmeEditor::setUiScale (float s)
{
    s = juce::jlimit (0.5f, 2.0f, s);
    setSize (juce::roundToInt ((float) MainView::baseWidth * s), juce::roundToInt ((float) MainView::baseHeight * s));
    ame::settings::set ("uiScale", juce::String (s, 3));
}

void AmeEditor::fitToScreen()
{
    if (! isShowing())
        return;
    const auto screen = getScreenBounds();
    const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (screen);
    if (display == nullptr)
        return;

    // Platz für Titelleiste/Rahmen des Host-Fensters lassen
    const auto area = display->userArea;
    const float maxW = (float) area.getWidth() - 16.0f, maxH = (float) area.getHeight() - 64.0f;
    if ((float) getWidth() <= maxW + 1.0f && (float) getHeight() <= maxH + 1.0f)
        return;

    const float fit = std::floor (juce::jmax (0.5f, juce::jmin (maxW / (float) MainView::baseWidth,
                                                                maxH / (float) MainView::baseHeight)) * 100.0f) / 100.0f;
    const juce::ScopedValueSetter<bool> svs (autoFitting, true);
    setSize (juce::roundToInt ((float) MainView::baseWidth * fit), juce::roundToInt ((float) MainView::baseHeight * fit));
}

AmeEditor::~AmeEditor() = default;

void AmeEditor::resized()
{
    const float s = (float) getWidth() / (float) MainView::baseWidth;
    view.setBounds (0, 0, MainView::baseWidth, MainView::baseHeight);
    view.setTransform (juce::AffineTransform::scale (s));
    view.setScaleDisplay (s);

    if (std::abs (s - scale) > 0.005f)
    {
        scale = s;
        if (isShowing() && ! autoFitting)   // nur Änderungen durch den Nutzer merken
            ame::settings::set ("uiScale", juce::String (s, 3));
    }
}
