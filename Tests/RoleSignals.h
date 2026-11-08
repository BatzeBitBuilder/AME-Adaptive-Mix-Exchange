#pragma once
// Synthetische Testsignale für die Rollenerkennung (Stereo)

#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>

struct StereoGen
{
    std::function<std::pair<float, float> (double t)> fn;
};

namespace testsig
{
inline float noise (juce::Random& r) { return r.nextFloat() * 2.0f - 1.0f; }
constexpr double twoPi = juce::MathConstants<double>::twoPi;

// Kick: 2 Schläge/s, Pitch-Sweep 150 -> 50 Hz, Klick
inline StereoGen kick()
{
    auto rnd = std::make_shared<juce::Random> (1);
    auto phase = std::make_shared<double> (0.0);
    return { [rnd, phase] (double t)
    {
        const double tt = std::fmod (t, 0.5);
        const double f = 50.0 + 100.0 * std::exp (-tt / 0.03);
        *phase += twoPi * f / 48000.0;
        float v = (float) (std::sin (*phase) * std::exp (-tt / 0.15));
        if (tt < 0.004) v += 0.3f * noise (*rnd);
        return std::pair { 0.8f * v, 0.8f * v };
    } };
}

// Snare: 1 Schlag/s (Offbeat), Ton 200 Hz + tiefpassgefiltertes Rauschen
inline StereoGen snare()
{
    auto rnd = std::make_shared<juce::Random> (2);
    auto lp = std::make_shared<float> (0.0f);
    return { [rnd, lp] (double t)
    {
        const double tt = std::fmod (t + 0.5, 1.0);
        const float env = (float) std::exp (-tt / 0.08);
        *lp += 0.35f * (noise (*rnd) - *lp);   // ~3 kHz Tiefpass
        const float v = env * (0.4f * (float) std::sin (twoPi * 200.0 * tt) + 1.2f * *lp);
        return std::pair { 0.6f * v, 0.6f * v };
    } };
}

// Hi-Hats: 8 Schläge/s, hochpassgefiltertes Rauschen, leicht stereo
inline StereoGen hats()
{
    auto rnd = std::make_shared<juce::Random> (3);
    auto lpL = std::make_shared<float> (0.0f), lpR = std::make_shared<float> (0.0f);
    return { [rnd, lpL, lpR] (double t)
    {
        const double tt = std::fmod (t, 0.125);
        const float env = (float) std::exp (-tt / 0.03);
        const float nl = noise (*rnd), nr = 0.7f * nl + 0.3f * noise (*rnd);
        *lpL += 0.2f * (nl - *lpL);
        *lpR += 0.2f * (nr - *lpR);
        return std::pair { 0.4f * env * (nl - *lpL), 0.4f * env * (nr - *lpR) };
    } };
}

// Bass: Sägezahn-artig (6 Harmonische), Töne 55/73/82/65 Hz je 0,5 s
inline StereoGen bass()
{
    auto phase = std::make_shared<double> (0.0);
    return { [phase] (double t)
    {
        static constexpr double notes[] { 55.0, 73.4, 82.4, 65.4 };
        const double f = notes[(int) (t / 0.5) % 4];
        *phase += twoPi * f / 48000.0;
        float v = 0.0f;
        for (int h = 1; h <= 6; ++h)
            v += (float) (std::sin (*phase * h) / h);
        const double tt = std::fmod (t, 0.5);
        const float env = (float) (0.6 + 0.4 * std::exp (-tt / 0.1));
        return std::pair { 0.35f * env * v, 0.35f * env * v };
    } };
}

// Gesang: f0 ~220 Hz mit Vibrato, Formanten ~700 Hz / 2,5 kHz, Phrasen 2 s an / 0,5 s aus
inline StereoGen vocal()
{
    auto phase = std::make_shared<double> (0.0);
    return { [phase] (double t)
    {
        static constexpr double melody[] { 220.0, 247.0, 262.0, 196.0 };
        const double f0 = melody[(int) (t / 1.25) % 4] * (1.0 + 0.015 * std::sin (twoPi * 5.5 * t));
        *phase += twoPi * f0 / 48000.0;
        float v = 0.0f;
        for (int h = 1; h <= 14; ++h)
        {
            const double fh = f0 * h;
            const double formant = std::exp (-std::pow (std::log2 (fh / 700.0), 2) * 2.0)
                                 + 0.5 * std::exp (-std::pow (std::log2 (fh / 2500.0), 2) * 3.0);
            v += (float) (formant * std::sin (*phase * h));
        }
        const double cycle = std::fmod (t, 2.5);
        const float env = cycle < 2.0 ? (float) juce::jmin (1.0, cycle / 0.05, (2.0 - cycle) / 0.05) : 0.0f;
        return std::pair { 0.3f * env * v, 0.3f * env * v };
    } };
}

// Pad: Akkord (A-Dur), weich, breit (L/R verstimmt)
inline StereoGen pad()
{
    return { [] (double t)
    {
        static constexpr double chord[] { 220.0, 277.2, 329.6, 440.0 };
        float l = 0.0f, r = 0.0f;
        for (auto f : chord)
            for (int h = 1; h <= 3; ++h)
            {
                l += (float) (std::sin (twoPi * f * h * t) / (h * h));
                r += (float) (std::sin (twoPi * f * 1.004 * h * t + 1.3 * h) / (h * h));
            }
        const float env = (float) (0.8 + 0.2 * std::sin (twoPi * 0.2 * t));
        return std::pair { 0.12f * env * l, 0.12f * env * r };
    } };
}
} // namespace testsig
