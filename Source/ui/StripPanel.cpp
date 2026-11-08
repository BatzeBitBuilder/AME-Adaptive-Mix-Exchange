// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "StripPanel.h"
#include "LookAndFeel.h"
#include "Parameters.h"
#include "i18n/Lang.h"
#include "engine/StripPresets.h"

namespace mtm::ui
{
using i18n::tr;

namespace
{
const juce::Identifier rackTabId ("rackTab");
const juce::Identifier presetNameId ("presetName");
constexpr int kFactoryIdBase = 1;
constexpr int kUserIdBase = 1000;
const juce::String userPrefix ("*");   // presetName-Präfix für eigene Presets
}

StripPanel::StripPanel (juce::AudioProcessorValueTreeState& state, MixHub& h, int s)
    : apvts (state), hub (h), slot (s), eqGraph (state, h, s)
{
    addAndMakeVisible (eqGraph);
    addAndMakeVisible (levelVu);
    addAndMakeVisible (grVu);
    addAndMakeVisible (waterfall);

    // INPUT
    input.title = "INPUT";
    input.knobs = { addKnob (pid::inGain, "Gain") };
    phaseLToggle = addToggle (pid::phaseL, MTM_TXT ("Ø L"));
    phaseRToggle = addToggle (pid::phaseR, MTM_TXT ("Ø R"));

    // FILTER
    filter.title = "FILTER";
    filter.knobs = { addKnob (pid::hpfFreq, "HPF"), addKnob (pid::lpfFreq, "LPF") };
    hpfToggle = addToggle (pid::hpfOn, "HPF");
    lpfToggle = addToggle (pid::lpfOn, "LPF");
    hpfSlope = addChoice (pid::hpfSlope, { "12 dB", "24 dB", "48 dB" });
    lpfSlope = addChoice (pid::lpfSlope, { "12 dB", "24 dB", "48 dB" });

    // Rack: Gate
    {
        auto& m = addModule (pid::gateOn, [] (const SlotAudioData& d) { return d.gateReductionDb.load (std::memory_order_relaxed); });
        gateOn = addToggle (pid::gateOn, {});
        gateMode = addChoice (pid::gateMode, { "Gate", "Expander 1:2", "Expander 1:4" });
        m.toggles = { gateOn };
        m.choices = { gateMode };
        m.knobs = { addKnob (pid::gateThresh, "Thresh"), addKnob (pid::gateRange, "Range"), addKnob (pid::gateAttack, "Attack"),
                    addKnob (pid::gateHold, "Hold"), addKnob (pid::gateRelease, "Release"), addKnob (pid::gateScHpf, "SC HPF") };
    }
    // Rack: Kompressor
    {
        auto& m = addModule (pid::compOn, [] (const SlotAudioData& d) { return d.gainReductionDb.load (std::memory_order_relaxed); });
        compOn = addToggle (pid::compOn, {});
        compAutoRel = addToggle (pid::compAutoRel, "Auto-Release");
        m.toggles = { compOn, compAutoRel };
        m.knobs = { addKnob (pid::compThresh, "Thresh"), addKnob (pid::compRatio, "Ratio"), addKnob (pid::compAttack, "Attack"),
                    addKnob (pid::compRelease, "Release"), addKnob (pid::compKnee, "Knee"), addKnob (pid::compMakeup, "Makeup"),
                    addKnob (pid::compMix, "Mix"), addKnob (pid::compScHpf, "SC HPF") };
    }
    // Rack: De-Esser
    {
        auto& m = addModule (pid::deessOn, [] (const SlotAudioData& d) { return d.deessReductionDb.load (std::memory_order_relaxed); });
        deessOn = addToggle (pid::deessOn, {});
        deessListen = addToggle (pid::deessListen, {});
        deessMode = addChoice (pid::deessMode, { "Split", "Wide" });
        m.toggles = { deessOn, deessListen };
        m.choices = { deessMode };
        m.knobs = { addKnob (pid::deessFreq, "Freq"), addKnob (pid::deessThresh, "Thresh"), addKnob (pid::deessRange, "Range") };
    }
    // Rack: Sättigung / Exciter
    {
        auto& m = addModule (pid::satOn, nullptr);
        satOn = addToggle (pid::satOn, {});
        satType = addChoice (pid::satType, {});
        m.toggles = { satOn };
        m.choices = { satType };
        m.knobs = { addKnob (pid::satDrive, "Drive"), addKnob (pid::satMix, "Mix"),
                    addKnob (pid::excAmount, "Exciter"), addKnob (pid::excFreq, "Exc Freq") };
    }
    // Rack: Limiter / Clipper
    {
        auto& m = addModule (pid::limOn, [] (const SlotAudioData& d) { return d.limiterReductionDb.load (std::memory_order_relaxed); });
        limOn = addToggle (pid::limOn, {});
        limMode = addChoice (pid::limMode, { "Limiter", "Soft Clip", "Hard Clip" });
        m.toggles = { limOn };
        m.choices = { limMode };
        m.knobs = { addKnob (pid::limCeiling, "Ceiling"), addKnob (pid::limRelease, "Release") };
    }
    for (int i = 0; i < (int) modules.size(); ++i)
    {
        auto& tab = modules[(size_t) i]->tab;
        tab.setClickingTogglesState (false);
        tab.onClick = [this, i] { showModule ((Module) i); };
        tab.getProperties().set ("lamp", false);   // Lampe im Reiter (LookAndFeel)
        addAndMakeVisible (tab);
    }

    // EQ
    eq.knobs = { addKnob (pid::lsFreq, "Low Hz"),  addKnob (pid::lsGain, "Low dB"), addKnob (pid::lsQ, "Low Q"),
                 addKnob (pid::p1Freq, "Mid1 Hz"), addKnob (pid::p1Gain, "Mid1 dB"), addKnob (pid::p1Q, "Mid1 Q"),
                 addKnob (pid::p2Freq, "Mid2 Hz"), addKnob (pid::p2Gain, "Mid2 dB"), addKnob (pid::p2Q, "Mid2 Q"),
                 addKnob (pid::hsFreq, "High Hz"), addKnob (pid::hsGain, "High dB"), addKnob (pid::hsQ, "High Q") };
    lowType = addChoice (pid::lsType, {});
    highType = addChoice (pid::hsType, {});
    order = addChoice (pid::chainOrder, {});
    for (auto* l : { &orderLabel, &lowTypeLabel, &highTypeLabel })
    {
        l->setFont (fonts::label (15.0f));
        l->setColour (juce::Label::textColourId, colours::textDim);
        l->setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (*l);
    }
    eq.title = "EQ";

    // OUTPUT
    output.title = "OUTPUT";
    output.knobs = { addKnob (pid::pan, "Pan"), addKnob (pid::width, "Width"), addKnob (pid::outGain, "Gain") };

    // ADAPTIV
    reactionKnob = addKnob (pid::reaction, {});
    adaptive.knobs = { addKnob (pid::amount, "Amount"), reactionKnob };
    aEqToggle   = addToggle (pid::adaptEq, "EQ");
    aHpfToggle  = addToggle (pid::adaptHpf, "HPF");
    aGainToggle = addToggle (pid::adaptGain, "Gain");
    aBypassToggle = addToggle (pid::adaptBypass, {});
    aFreezeToggle = addToggle (pid::adaptFreeze, "Hold");

    priorityLabel.setFont (fonts::label (15.0f));
    priorityLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (priorityLabel);
    priorityBox.addItemList (getPriorityNames(), 1);
    addAndMakeVisible (priorityBox);
    priorityAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, pid::priority, priorityBox);

    // Presets
    presetLabel.setFont (fonts::label (17.0f));
    presetLabel.setColour (juce::Label::textColourId, colours::ink);
    presetLabel.setText ("PRESET", juce::dontSendNotification);
    addAndMakeVisible (presetLabel);
    presetBox.onChange = [this] { presetChosen(); };
    addAndMakeVisible (presetBox);
    fromRoleButton.onClick = [this] { loadPresetForCurrentRole(); };
    addAndMakeVisible (fromRoleButton);
    saveButton.onClick = [this] { askSavePreset(); };
    deleteButton.onClick = [this] { askDeletePreset(); };
    abA.onClick = [this] { presets::selectAB (apvts, "A"); updatePresetButtons(); };
    abB.onClick = [this] { presets::selectAB (apvts, "B"); updatePresetButtons(); };
    abCopy.onClick = [this] { presets::copyActiveToOther (apvts); };
    abA.setConnectedEdges (juce::Button::ConnectedOnRight);
    abB.setConnectedEdges (juce::Button::ConnectedOnLeft);
    for (auto* b : { &saveButton, &deleteButton, &abA, &abB, &abCopy })
        addAndMakeVisible (*b);

    const int savedTab = (int) apvts.state.getProperty (rackTabId, (int) Module::Compressor);
    showModule ((Module) juce::jlimit (0, (int) modules.size() - 1, savedTab));
    updateTexts();
}

void StripPanel::updateTexts()
{
    static const std::pair<const char*, const char*> tabNames[] {
        { "GATE", "GATE" }, { "KOMPRESSOR", "COMPRESSOR" }, { "DE-ESSER", "DE-ESSER" }, { "SÄTTIGUNG", "SATURATION" }, { "LIMITER", "LIMITER" } };
    for (size_t i = 0; i < modules.size(); ++i)
        modules[i]->tab.setButtonText (tr (tabNames[i].first, tabNames[i].second));

    auto refill = [] (juce::ComboBox& box, const juce::StringArray& items)
    {
        const int sel = box.getSelectedItemIndex();
        box.clear (juce::dontSendNotification);
        box.addItemList (items, 1);
        box.setSelectedItemIndex (juce::jmax (0, sel), juce::dontSendNotification);
    };
    refill (satType->box, { tr ("Röhre", "Tube"), tr ("Band", "Tape"), "Transistor" });
    refill (lowType->box, { "Shelf", tr ("Glocke", "Bell") });
    refill (highType->box, { "Shelf", tr ("Glocke", "Bell") });
    refill (order->box, { "EQ > Comp", "Comp > EQ" });
    orderLabel.setText (tr ("Reihenfolge", "Order"), juce::dontSendNotification);
    lowTypeLabel.setText ("Low", juce::dontSendNotification);
    highTypeLabel.setText ("High", juce::dontSendNotification);
    satType->box.setTooltip (tr ("Röhre: weich, gerade Obertöne. Band: weich, symmetrisch. Transistor: härter, ungerade Obertöne.",
                                 "Tube: soft, even harmonics. Tape: soft, symmetric. Transistor: harder, odd harmonics."));
    hpfSlope->box.setTooltip (tr ("Flankensteilheit Hochpass", "High-pass slope"));
    lpfSlope->box.setTooltip (tr ("Flankensteilheit Tiefpass", "Low-pass slope"));

    limMode->box.setTooltip (tr ("Limiter: Spitzenbegrenzer ohne Latenz (Release). Soft Clip: weiche Sättigung zum Ceiling. Hard Clip: hart abgeschnitten. Sitzt hinter Output-Gain.",
                                 "Limiter: zero-latency peak limiter (release). Soft clip: soft saturation towards the ceiling. Hard clip: hard cut. Sits after the output gain."));
    saveButton.setButtonText (tr ("Speichern", "Save"));
    deleteButton.setButtonText (tr ("Löschen", "Delete"));
    abCopy.setButtonText (MTM_TXT ("A→B"));
    saveButton.setTooltip (tr ("Aktuelle Einstellungen als eigenes Preset speichern (alle Instanzen, alle Projekte)",
                               "Save the current settings as a user preset (all instances, all projects)"));
    deleteButton.setTooltip (tr ("Geladenes eigenes Preset löschen", "Delete the loaded user preset"));
    abA.setTooltip (tr ("Einstellung A (A/B-Vergleich)", "Setting A (A/B comparison)"));
    abB.setTooltip (tr ("Einstellung B (A/B-Vergleich)", "Setting B (A/B comparison)"));
    abCopy.setTooltip (tr ("Aktive Einstellung in den anderen Platz kopieren", "Copy the active setting to the other slot"));

    for (auto* t : { gateOn, compOn, deessOn, satOn, limOn })
        t->button.setButtonText (tr ("An", "On"));
    deessListen->button.setButtonText (tr ("Abhören", "Listen"));
    gateMode->box.setTooltip (tr ("Gate: schließt unter dem Threshold um Range. Expander: senkt leise Anteile im Verhältnis 1:2 / 1:4 ab.",
                                  "Gate: closes by Range below the threshold. Expander: lowers quiet parts with a 1:2 / 1:4 ratio."));
    deessMode->box.setTooltip (tr ("Split: nur das Zischband wird abgesenkt. Wide: das ganze Signal.",
                                   "Split: only the sibilance band is reduced. Wide: the whole signal."));
    phaseLToggle->button.setTooltip (tr ("Phase (Polarität) links umkehren", "Invert phase (polarity) left"));
    phaseRToggle->button.setTooltip (tr ("Phase (Polarität) rechts umkehren", "Invert phase (polarity) right"));

    adaptive.title = tr ("ADAPTIV", "ADAPTIVE");
    reactionKnob->label.setText (tr ("Ansprechzeit", "Response"), juce::dontSendNotification);
    aBypassToggle->button.setButtonText (tr ("Direkt", "Direct"));
    priorityLabel.setText (tr ("Vorrang", "Priority"), juce::dontSendNotification);

    // Einträge neu, Auswahl bleibt (Parameter unverändert)
    const int sel = priorityBox.getSelectedItemIndex();
    priorityBox.clear (juce::dontSendNotification);
    priorityBox.addItemList (getPriorityNames(), 1);
    priorityBox.setSelectedItemIndex (sel, juce::dontSendNotification);

    fillPresetBox();
    updateRoleButton();

    levelVu.setCaption (tr ("Ausgang", "Output"));
    grVu.setCaption (tr ("Kompressor", "Compressor"));
    waterfall.setCaption (tr ("SPEKTROGRAMM", "SPECTROGRAM"));
    plateCounter = 14;   // Typenschild beim nächsten Meter-Update neu
    if (! getLocalBounds().isEmpty())
        resized();       // Breiten der Schalter folgen dem Text
    repaint();
}

StripPanel::~StripPanel() = default;

StripPanel::Knob* StripPanel::addKnob (const char* paramId, const juce::String& label)
{
    auto k = std::make_unique<Knob>();
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    k->label.setText (label, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (fonts::label (15.0f));
    k->label.setColour (juce::Label::textColourId, colours::textDim);
    k->label.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, k->slider);

    auto* raw = k.get();
    knobs.push_back (std::move (k));
    return raw;
}

StripPanel::Toggle* StripPanel::addToggle (const char* paramId, const juce::String& label)
{
    auto t = std::make_unique<Toggle>();
    t->button.setButtonText (label);
    addAndMakeVisible (t->button);
    t->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, paramId, t->button);

    auto* raw = t.get();
    toggles.push_back (std::move (t));
    return raw;
}

StripPanel::Choice* StripPanel::addChoice (const char* paramId, const juce::StringArray& items)
{
    auto c = std::make_unique<Choice>();
    c->box.addItemList (items, 1);
    addAndMakeVisible (c->box);
    c->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramId, c->box);

    auto* raw = c.get();
    choices.push_back (std::move (c));
    return raw;
}

StripPanel::RackModule& StripPanel::addModule (const char* onParam, std::function<float (const SlotAudioData&)> reduction)
{
    auto m = std::make_unique<RackModule>();
    m->onParam = onParam;
    m->reduction = std::move (reduction);
    modules.push_back (std::move (m));
    return *modules.back();
}

void StripPanel::fillPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading (tr ("Werkspresets je Rolle", "Factory presets by role"));
    const auto& factory = presets::getFactoryPresets();
    for (int i = 0; i < (int) factory.size(); ++i)
        presetBox.addItem (getRoleNames()[(int) factory[(size_t) i].role], kFactoryIdBase + i);

    const auto user = presets::getUserPresetNames();
    if (! user.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading (tr ("Eigene Presets", "User presets"));
        for (int i = 0; i < user.size(); ++i)
            presetBox.addItem (user[i], kUserIdBase + i);
    }

    juce::String shown = apvts.state.getProperty (presetNameId).toString();
    if (shown.startsWith (userPrefix))
        shown = shown.substring (userPrefix.length());
    presetBox.setTextWhenNothingSelected (shown.isNotEmpty() ? shown : tr ("– kein Preset –", "– no preset –"));
    updatePresetButtons();
    presetBox.setTooltip (tr ("Werkspreset laden: setzt Filter, EQ, Gate, Kompressor und De-Esser. Input, Phase und Output bleiben.",
                              "Load a factory preset: sets filter, EQ, gate, compressor and de-esser. Input, phase and output stay."));
}

void StripPanel::presetChosen()
{
    const int id = presetBox.getSelectedId();
    const auto& factory = presets::getFactoryPresets();
    const int fi = id - kFactoryIdBase;
    if (juce::isPositiveAndBelow (fi, (int) factory.size()))
    {
        presets::applyFactory (apvts, factory[(size_t) fi]);
        apvts.state.setProperty (presetNameId, tr ("Werk: ", "Factory: ") + presetBox.getText(), nullptr);
    }
    else if (id >= kUserIdBase)
    {
        const auto name = presetBox.getText();
        if (presets::loadUserPreset (apvts, name))
            apvts.state.setProperty (presetNameId, userPrefix + name, nullptr);
    }
    // Anzeige: Name des geladenen Presets, Auswahl zurücksetzen (erneut wählbar)
    presetBox.setSelectedId (0, juce::dontSendNotification);
    fillPresetBox();
}

void StripPanel::loadPresetForCurrentRole()
{
    if (const auto* fp = presets::findFactoryPreset (currentRole))
    {
        presets::applyFactory (apvts, *fp);
        apvts.state.setProperty (presetNameId, tr ("Werk: ", "Factory: ") + getRoleNames()[(int) currentRole], nullptr);
        fillPresetBox();
    }
}

void StripPanel::updatePresetButtons()
{
    const bool user = apvts.state.getProperty (presetNameId).toString().startsWith (userPrefix);
    deleteButton.setEnabled (user);
    const bool b = presets::getActiveAB (apvts) == "B";
    abA.setToggleState (! b, juce::dontSendNotification);
    abB.setToggleState (b, juce::dontSendNotification);
    abCopy.setButtonText (b ? MTM_TXT ("B→A") : MTM_TXT ("A→B"));
}

void StripPanel::askSavePreset()
{
    juce::String suggestion = apvts.state.getProperty (presetNameId).toString();
    if (suggestion.startsWith (userPrefix))
        suggestion = suggestion.substring (userPrefix.length());
    else
        suggestion = {};

    auto* w = new juce::AlertWindow (tr ("Preset speichern", "Save preset"),
                                     tr ("Name des eigenen Presets (gleicher Name überschreibt):",
                                         "Name of the user preset (same name overwrites):"),
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", suggestion);
    w->addButton (tr ("Speichern", "Save"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (tr ("Abbrechen", "Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<StripPanel> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (safe == nullptr || result != 1)
            return;
        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isNotEmpty() && presets::saveUserPreset (safe->apvts, name))
        {
            safe->apvts.state.setProperty (presetNameId, userPrefix + name, nullptr);
            safe->fillPresetBox();
        }
    }), true);
}

void StripPanel::askDeletePreset()
{
    auto name = apvts.state.getProperty (presetNameId).toString();
    if (! name.startsWith (userPrefix))
        return;
    name = name.substring (userPrefix.length());

    juce::Component::SafePointer<StripPanel> safe (this);
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, tr ("Preset löschen", "Delete preset"),
                                        tr ("Eigenes Preset löschen: ", "Delete user preset: ") + name,
                                        tr ("Löschen", "Delete"), tr ("Abbrechen", "Cancel"), this,
                                        juce::ModalCallbackFunction::create ([safe, name] (int result)
    {
        if (safe == nullptr || result == 0)
            return;
        presets::deleteUserPreset (name);
        safe->apvts.state.setProperty (presetNameId, {}, nullptr);
        safe->fillPresetBox();
    }));
}

void StripPanel::updateRoleButton()
{
    const bool ok = presets::findFactoryPreset (currentRole) != nullptr;
    fromRoleButton.setEnabled (ok);
    fromRoleButton.setButtonText (ok ? tr ("Preset der Rolle: ", "Role preset: ") + getRoleNames()[(int) currentRole]
                                     : tr ("Preset der Rolle (Rolle unbekannt)", "Role preset (role unknown)"));
    fromRoleButton.setTooltip (tr ("Lädt das Werkspreset der eingestellten bzw. automatisch erkannten Rolle.",
                                   "Loads the factory preset of the selected or automatically detected role."));
}

void StripPanel::showModule (Module m)
{
    currentModule = m;
    apvts.state.setProperty (rackTabId, (int) m, nullptr);

    for (size_t i = 0; i < modules.size(); ++i)
    {
        auto& mod = *modules[i];
        const bool vis = (int) i == (int) m;
        mod.tab.setToggleState (vis, juce::dontSendNotification);
        for (auto* t : mod.toggles) t->button.setVisible (vis);
        for (auto* c : mod.choices) c->box.setVisible (vis);
        for (auto* k : mod.knobs)   { k->slider.setVisible (vis); k->label.setVisible (vis); }
    }
    repaint (rackBounds);
}

void StripPanel::layoutKnobRow (const std::vector<Knob*>& row, juce::Rectangle<int> r)
{
    if (row.empty())
        return;
    const int w = r.getWidth() / (int) row.size();
    for (auto* k : row)
    {
        auto cell = r.removeFromLeft (w);
        k->label.setBounds (cell.removeFromTop (16));
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmin (74, cell.getWidth()), 16);
        k->slider.setBounds (cell);
    }
}

void StripPanel::layoutRack (juce::Rectangle<int> r)
{
    rackBounds = r;
    auto inner = r.reduced (6);

    // Reiter
    auto tabs = inner.removeFromTop (24);
    for (auto& m : modules)
    {
        const int w = juce::jmax (78, m->tab.getBestWidthForHeight (24) + 30);
        m->tab.setBounds (tabs.removeFromLeft (w).reduced (1, 0));
        m->tab.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    }

    // Reduktionsanzeige rechts
    rackMeter = inner.removeFromRight (26).withTrimmedTop (6);
    inner.removeFromRight (6);

    // Steuerzeile: Schalter + Auswahl je Modul
    inner.removeFromTop (6);
    auto ctrl = inner.removeFromTop (24);
    for (auto& m : modules)
    {
        auto c = ctrl;
        c.removeFromLeft (6);
        for (auto* t : m->toggles)
            t->button.setBounds (c.removeFromLeft (juce::jmax (56, juce::roundToInt (juce::GlyphArrangement::getStringWidth (fonts::label (16.0f), t->button.getButtonText())) + 32)));
        c.removeFromLeft (10);
        for (auto* ch : m->choices)
        {
            ch->box.setBounds (c.removeFromLeft (150).reduced (0, 1));
            c.removeFromLeft (8);
        }
    }

    inner.removeFromTop (4);
    for (auto& m : modules)
    {
        // Knöpfe gleich breit über alle Module (max. 8 je Zeile)
        const int cellW = inner.getWidth() / 8;
        auto row = inner.withWidth (cellW * (int) m->knobs.size());
        layoutKnobRow (m->knobs, row);
    }
}

void StripPanel::resized()
{
    auto area = getLocalBounds().reduced (8);
    constexpr int gap = 6;

    // Rechte Spalte: VU-Meter, Typenschild, OUTPUT
    auto meterCol = area.removeFromRight (176);
    area.removeFromRight (8);
    levelVu.setBounds (meterCol.removeFromTop (128));
    meterCol.removeFromTop (8);
    grVu.setBounds (meterCol.removeFromTop (128));
    meterCol.removeFromTop (10);
    plateArea = meterCol.removeFromTop (150);
    meterCol.removeFromTop (gap);
    {
        output.bounds = meterCol;
        auto inner = meterCol.reduced (6);
        inner.removeFromTop (22);
        const int h = juce::jmin (130, inner.getHeight() / 2);
        layoutKnobRow ({ output.knobs[0], output.knobs[1] }, inner.removeFromTop (h));
        inner.removeFromTop (6);
        layoutKnobRow ({ output.knobs[2] }, inner.removeFromTop (h).withSizeKeepingCentre (88, h));
        outputSpare = inner;
    }

    // Oben: Preset-Leiste
    presetBar = area.removeFromTop (34);
    {
        auto r = presetBar.reduced (8, 4);
        presetLabel.setBounds (r.removeFromLeft (62));
        presetBox.setBounds (r.removeFromLeft (230));
        r.removeFromLeft (8);
        fromRoleButton.setBounds (r.removeFromLeft (226));
        r.removeFromLeft (8);
        saveButton.setBounds (r.removeFromLeft (84));
        r.removeFromLeft (4);
        deleteButton.setBounds (r.removeFromLeft (72));
        abCopy.setBounds (r.removeFromRight (52));
        r.removeFromRight (4);
        abB.setBounds (r.removeFromRight (32));
        abA.setBounds (r.removeFromRight (32));
        presetTools = r;
    }
    area.removeFromTop (gap);

    // Unten: Adaptiv
    auto row3 = area.removeFromBottom (150);
    area.removeFromBottom (gap);

    // Oben: INPUT | FILTER | Rack
    auto row1 = area.removeFromTop (204);
    area.removeFromTop (gap);

    {
        input.bounds = row1.removeFromLeft (104);
        auto inner = input.bounds.reduced (6);
        inner.removeFromTop (22);
        auto ph = inner.removeFromTop (22);
        phaseLToggle->button.setBounds (ph.removeFromLeft (ph.getWidth() / 2));
        phaseRToggle->button.setBounds (ph);
        inner.removeFromTop (4);
        layoutKnobRow (input.knobs, inner);
    }
    row1.removeFromLeft (gap);
    {
        filter.bounds = row1.removeFromLeft (178);
        auto inner = filter.bounds.reduced (6);
        auto t = inner.removeFromTop (22);
        lpfToggle->button.setBounds (t.removeFromRight (56));
        hpfToggle->button.setBounds (t.removeFromRight (56));
        inner.removeFromTop (4);
        auto sl = inner.removeFromTop (22);
        hpfSlope->box.setBounds (sl.removeFromLeft (sl.getWidth() / 2).reduced (6, 0));
        lpfSlope->box.setBounds (sl.reduced (6, 0));
        layoutKnobRow (filter.knobs, inner);
    }
    row1.removeFromLeft (gap);
    layoutRack (row1);

    // EQ: Analyzer, Spektrogramm, Regler
    {
        eq.bounds = area;
        auto inner = area.reduced (6);
        auto t = inner.removeFromTop (22);
        highType->box.setBounds (t.removeFromRight (104).reduced (0, 1));
        highTypeLabel.setBounds (t.removeFromRight (44));
        t.removeFromRight (10);
        lowType->box.setBounds (t.removeFromRight (104).reduced (0, 1));
        lowTypeLabel.setBounds (t.removeFromRight (40));
        t.removeFromRight (16);
        order->box.setBounds (t.removeFromRight (136).reduced (0, 1));
        orderLabel.setBounds (t.removeFromRight (90));
        inner.removeFromTop (2);
        auto knobRow = inner.removeFromBottom (122);
        inner.removeFromBottom (4);
        auto wf = inner.removeFromBottom (58);
        inner.removeFromBottom (4);
        eqGraph.setBounds (inner);
        waterfall.setBounds (wf.withTrimmedLeft (24));   // an Analyzer-Achse ausrichten
        layoutKnobRow (eq.knobs, knobRow);
    }

    // Adaptiv
    adaptive.bounds = row3;
    auto inner = row3.reduced (6);
    auto title = inner.removeFromTop (22);
    title.removeFromLeft (84);
    aEqToggle->button.setBounds (title.removeFromLeft (52));
    aHpfToggle->button.setBounds (title.removeFromLeft (58));
    aGainToggle->button.setBounds (title.removeFromLeft (64));
    title.removeFromLeft (16);
    aBypassToggle->button.setBounds (title.removeFromLeft (104));
    aFreezeToggle->button.setBounds (title.removeFromLeft (104));

    auto knobArea = inner.removeFromLeft (160);
    layoutKnobRow (adaptive.knobs, knobArea);

    inner.removeFromLeft (10);
    auto prioArea = inner.removeFromLeft (150);
    prioArea.removeFromTop (8);
    priorityLabel.setBounds (prioArea.removeFromTop (18));
    priorityBox.setBounds (prioArea.removeFromTop (26));

    inner.removeFromLeft (16);
    adaptiveDisplay = inner.reduced (0, 2);
}

void StripPanel::paint (juce::Graphics& g)
{
    for (auto* s : { &input, &filter, &eq, &output, &adaptive })
        drawEngravedPanel (g, s->bounds.toFloat(), s->title);
    drawEngravedPanel (g, presetBar.toFloat(), {});

    paintRack (g);
    paintAdaptive (g);
    paintPlate (g);
}

void StripPanel::paintRack (juce::Graphics& g)
{
    if (rackBounds.isEmpty())
        return;

    drawEngravedPanel (g, rackBounds.toFloat(), {});

    // Reduktionsanzeige (LED-Kette) des sichtbaren Moduls
    auto r = rackMeter.toFloat();
    drawDisplayBezel (g, r);
    auto led = r.reduced (3.0f, 4.0f);
    auto caption = led.removeFromBottom (12.0f);
    g.setColour (colours::phosphorDim);
    g.setFont (fonts::mono (9.0f, true));
    g.drawText ("GR", caption, juce::Justification::centred);

    const auto& cur = *modules[(size_t) currentModule];
    if (! cur.reduction)
        return;   // Modul ohne Pegelreduktion (Sättigung): Anzeige dunkel
    const float db = cur.meterDb;
    constexpr int segs = 12;            // 2 dB je Segment, 0 … 24 dB
    const float segH = led.getHeight() / (float) segs;
    for (int i = 0; i < segs; ++i)
    {
        auto sr = led.withY (led.getY() + segH * (float) i).withHeight (segH).reduced (0.0f, 1.0f);
        const bool on = db > (float) i * 2.0f + 0.25f;
        g.setColour (on ? maskingColour ((float) i / (float) (segs - 1)) : colours::displayGrid.withAlpha (0.6f));
        g.fillRect (sr);
    }
}

void StripPanel::paintPlate (juce::Graphics& g)
{
    if (plateArea.isEmpty())
        return;

    {
        auto r = plateArea.toFloat();
        g.setColour (colours::bakelite);
        g.fillRoundedRectangle (r, 5.0f);
        auto inner = r.reduced (5.0f);
        juce::ColourGradient brass (juce::Colour (0xffd9b56a), inner.getX(), inner.getY(),
                                    juce::Colour (0xff9c7a3a), inner.getRight(), inner.getBottom(), false);
        g.setGradientFill (brass);
        g.fillRoundedRectangle (inner, 3.0f);
        for (auto p : { inner.getTopLeft().translated (7.0f, 7.0f), inner.getTopRight().translated (-7.0f, 7.0f),
                        inner.getBottomLeft().translated (7.0f, -7.0f), inner.getBottomRight().translated (-7.0f, -7.0f) })
            drawScrew (g, p, 3.5f);

        auto t = inner.reduced (14.0f, 12.0f);
        g.setColour (colours::bakelite);

        // Kopfzeile: Hersteller-Gravur + Line-Nummer
        auto head = t.removeFromTop (14.0f);
        g.setFont (juce::Font (fonts::label (13.0f)).withExtraKerningFactor (0.12f));
        g.drawText ("AME  MODEL " JucePlugin_VersionString, head, juce::Justification::centredLeft);
        g.drawText (plateLine, head, juce::Justification::centredRight);
        t.removeFromTop (3.0f);
        g.setColour (colours::bakelite.withAlpha (0.45f));
        g.fillRect (t.removeFromTop (1.0f));
        t.removeFromTop (3.0f);

        g.setColour (colours::bakelite);
        g.setFont (fonts::display (18.0f));
        g.drawText (plateName, t.removeFromTop (24.0f), juce::Justification::centredLeft, true);
        t.removeFromTop (2.0f);
        g.setFont (fonts::label (13.0f));
        g.drawText (tr ("ROLLE", "ROLE"), t.removeFromTop (13.0f), juce::Justification::centredLeft);
        g.setFont (fonts::mono (13.0f, true));
        g.drawText (plateRole, t.removeFromTop (17.0f), juce::Justification::centredLeft, true);
        t.removeFromTop (2.0f);
        g.setFont (fonts::label (13.0f));
        g.drawText (tr ("PROGRAMM", "PROGRAM"), t.removeFromTop (13.0f), juce::Justification::centredLeft);
        g.setFont (fonts::mono (13.0f, true));
        g.drawText (plateGenre, t.removeFromTop (17.0f), juce::Justification::centredLeft, true);
    }
}

void StripPanel::paintAdaptive (juce::Graphics& g)
{
    auto area = adaptiveDisplay.toFloat();
    if (area.isEmpty())
        return;

    drawDisplayBezel (g, area);
    area.reduce (8.0f, 5.0f);

    // Statuszeile
    auto status = area.removeFromTop (16.0f);
    juce::String st;
    juce::Colour sc = colours::phosphor;
    if (bypassed)          { st = tr ("DIREKT – EINGRIFFE AUS", "DIRECT – PROCESSING OFF"); sc = colours::amberLamp; }
    else if (frozen)       { st = tr ("HOLD – WERTE GEHALTEN", "HOLD – VALUES FROZEN");     sc = juce::Colour (0xff8fd0ff); }
    else if (engineAlive)  { st = tr ("EXCHANGE VERBUNDEN", "EXCHANGE CONNECTED"); }
    else                   { st = tr ("NO EXCHANGE – LINE LÄUFT SOLO", "NO EXCHANGE – LINE RUNS SOLO"); sc = colours::amberLamp; }
    g.setFont (fonts::mono (12.0f, true));
    g.setColour (sc);
    g.drawText (st, status, juce::Justification::centredLeft);

    juce::String info;
    info << "HPF " << (hpfHz >= 20.0f ? juce::String (juce::roundToInt (hpfHz)) + " Hz" : tr ("AUS", "OFF"))
         << "   GAIN " << (gainDb >= 0.0f ? "+" : "") << juce::String (gainDb, 1) << " dB";
    g.setColour (colours::phosphor);
    g.drawText (info, status, juce::Justification::centredRight);

    area.removeFromTop (4.0f);
    auto labels = area.removeFromBottom (13.0f);
    auto plot = area;

    constexpr float rangeDb = 9.0f;
    static const char* names[kNumAdaptiveBands] { "31", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k" };
    const float w = plot.getWidth() / (float) kNumAdaptiveBands;

    g.setColour (colours::displayGrid);
    for (int i = 1; i < 3; ++i)
        g.drawHorizontalLine (juce::roundToInt (plot.getY() + plot.getHeight() * (float) i / 3.0f), plot.getX(), plot.getRight());

    for (int k = 0; k < kNumAdaptiveBands; ++k)
    {
        const float cut = juce::jlimit (-rangeDb, 0.0f, cutDb[(size_t) k]);
        auto cell = juce::Rectangle<float> (plot.getX() + w * (float) k, plot.getY(), w, plot.getHeight()).reduced (4.0f, 0.0f);
        const float frac = -cut / rangeDb;

        // segmentierte LED-Säule von oben (Absenkung)
        const int segs = 9;
        const float segH = cell.getHeight() / (float) segs;
        for (int sgm = 0; sgm < segs; ++sgm)
        {
            auto sr = cell.withY (cell.getY() + segH * (float) sgm).withHeight (segH).reduced (0.0f, 1.0f);
            const bool on = (float) sgm < frac * (float) segs - 0.01f;
            g.setColour (on ? maskingColour ((float) sgm / (float) (segs - 1)) : colours::displayGrid.withAlpha (0.6f));
            g.fillRect (sr);
        }

        g.setColour (colours::phosphorDim);
        g.setFont (fonts::mono (10.0f, true));
        g.drawText (names[k], juce::Rectangle<float> (cell.getX() - 4.0f, labels.getY(), cell.getWidth() + 8.0f, 13.0f),
                    juce::Justification::centred);
    }
}

void StripPanel::refreshMeters()
{
    constexpr float dt = 1.0f / 30.0f;
    if (auto* d = hub.getAudioData (slot))
    {
        levelVu.update (d->rmsDb.load(), d->peakDb.load(), dt);
        grVu.updateGainReduction (d->gainReductionDb.load(), dt);
        for (size_t k = 0; k < (size_t) kNumAdaptiveBands; ++k)
            cutDb[k] = d->appliedCutDb[k].load();
        hpfHz = d->appliedHpfHz.load();
        gainDb = d->appliedGainDb.load();
        engineAlive = hub.isEngineAlive();
        frozen = d->frozen.load();
        bypassed = d->bypassed.load();
        eqGraph.refresh (dt);

        // Rack: Reduktion (Anzeige mit schnellem Anstieg, langsamem Abfall) und Lampen der Reiter
        for (auto& m : modules)
        {
            const float v = m->reduction ? m->reduction (*d) : 0.0f;
            m->meterDb = v > m->meterDb ? v : juce::jmax (v, m->meterDb - 30.0f * dt);
            const bool on = m->onParam != nullptr && apvts.getRawParameterValue (m->onParam)->load() > 0.5f;
            if ((bool) m->tab.getProperties()["lamp"] != on)
            {
                m->tab.getProperties().set ("lamp", on);
                m->tab.repaint();
            }
        }
        repaint (rackMeter.expanded (4));

        // Typenschild (2 Hz genügt)
        if (++plateCounter % 15 == 0)
        {
            for (const auto& t : hub.getSnapshot())
                if (t.slot == slot)
                {
                    plateName = t.name;
                    plateRole = getRoleNames()[(int) t.role] + (t.autoRole ? tr (" (Kennung)", " (auto)") : juce::String());
                    plateLine = "LINE " + juce::String (t.instanceId).paddedLeft ('0', 2);
                    if (t.role != currentRole)
                    {
                        currentRole = t.role;
                        updateRoleButton();
                    }
                }
            plateGenre = hub.isEngineAlive() ? getGenreNames()[hub.globalGenre.load()] : MTM_TXT ("– no exchange –");
            repaint (plateArea);
        }
        waterfall.pushRow (eqGraph.getSpectrum());
        repaint (adaptiveDisplay.expanded (6));
    }
}
} // namespace mtm::ui
