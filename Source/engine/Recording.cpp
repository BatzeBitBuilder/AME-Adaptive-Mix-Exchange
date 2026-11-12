// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "Recording.h"

namespace ame
{
namespace
{
int16_t enc (float v, float scale) noexcept
{
    return (int16_t) juce::jlimit (-32767, 32767, juce::roundToInt (v * scale));
}
constexpr float cutScale = 100.0f, hpfScale = 10.0f, gainScale = 100.0f;
constexpr int searchFrames = 10 * AdaptiveRecording::kFramesPerSecond;   // Lücken bis 10 s überbrücken
} // namespace

void AdaptiveRecording::allocate()
{
    if (isAllocated())
        return;
    const auto n = (size_t) kMaxFrames * (size_t) kValues;
    storage.reset (new std::atomic<int16_t>[n]);
    for (size_t i = 0; i < n; ++i)
        storage[i].store (kInvalid, std::memory_order_relaxed);
    numFrames.store (0);
    data.store (storage.get(), std::memory_order_release);
}

void AdaptiveRecording::clear()
{
    if (auto* d = data.load (std::memory_order_acquire))
    {
        const auto n = (size_t) numFrames.load() * (size_t) kValues;
        for (size_t i = 0; i < n; ++i)
            d[i].store (kInvalid, std::memory_order_relaxed);
    }
    numFrames.store (0);
    revision.fetch_add (1, std::memory_order_relaxed);
}

bool AdaptiveRecording::frameValid (int f) const noexcept
{
    auto* d = data.load (std::memory_order_acquire);
    return d != nullptr && f >= 0 && f < numFrames.load (std::memory_order_relaxed)
           && d[(size_t) f * kValues].load (std::memory_order_relaxed) != kInvalid;
}

void AdaptiveRecording::decode (int f, Values& v) const noexcept
{
    auto* d = data.load (std::memory_order_acquire) + (size_t) f * kValues;
    for (size_t k = 0; k < (size_t) kNumAdaptiveBands; ++k)
        v.cutDb[k] = (float) d[k].load (std::memory_order_relaxed) / cutScale;
    v.hpfHz  = (float) d[kNumAdaptiveBands].load (std::memory_order_relaxed) / hpfScale;
    v.gainDb = (float) d[kNumAdaptiveBands + 1].load (std::memory_order_relaxed) / gainScale;
}

void AdaptiveRecording::write (double startSec, double durationSec, const Values& v) noexcept
{
    auto* d = data.load (std::memory_order_acquire);
    if (d == nullptr || startSec < 0.0)
        return;

    // alle Frames, die in diesen Block fallen (mindestens einer)
    const int first = (int) std::floor (startSec * kFramesPerSecond);
    const int last = juce::jmin (kMaxFrames - 1, juce::jmax (first, (int) std::ceil ((startSec + durationSec) * kFramesPerSecond) - 1));
    if (first >= kMaxFrames)
        return;

    for (int f = first; f <= last; ++f)
    {
        auto* p = d + (size_t) f * kValues;
        for (size_t k = 0; k < (size_t) kNumAdaptiveBands; ++k)
            p[k].store (enc (v.cutDb[k], cutScale), std::memory_order_relaxed);
        p[kNumAdaptiveBands].store (enc (v.hpfHz, hpfScale), std::memory_order_relaxed);
        p[kNumAdaptiveBands + 1].store (enc (v.gainDb, gainScale), std::memory_order_relaxed);
    }

    if (last + 1 > numFrames.load (std::memory_order_relaxed))
        numFrames.store (last + 1, std::memory_order_relaxed);
    revision.fetch_add (1, std::memory_order_relaxed);
}

bool AdaptiveRecording::read (double timeSec, Values& v) const noexcept
{
    const int frames = numFrames.load (std::memory_order_relaxed);
    if (frames == 0 || data.load (std::memory_order_acquire) == nullptr)
        return false;

    const double x = juce::jmax (0.0, timeSec * kFramesPerSecond);
    const int f0 = (int) std::floor (x);

    // nächster gültiger Frame davor / danach (Lücken, z. B. große Audio-Blöcke beim Aufnehmen)
    int a = -1, b = -1;
    for (int i = juce::jmin (f0, frames - 1); i >= 0 && i >= f0 - searchFrames; --i)
        if (frameValid (i)) { a = i; break; }
    for (int i = juce::jmax (0, f0 + 1); i < frames && i <= f0 + searchFrames; ++i)
        if (frameValid (i)) { b = i; break; }

    if (a < 0 && b < 0)
        return false;
    if (a < 0) { decode (b, v); return true; }
    if (b < 0) { decode (a, v); return true; }

    Values va, vb;
    decode (a, va);
    decode (b, vb);
    const float t = (float) juce::jlimit (0.0, 1.0, (x - a) / (double) (b - a));
    for (size_t k = 0; k < (size_t) kNumAdaptiveBands; ++k)
        v.cutDb[k] = va.cutDb[k] + t * (vb.cutDb[k] - va.cutDb[k]);
    v.hpfHz  = va.hpfHz  + t * (vb.hpfHz  - va.hpfHz);
    v.gainDb = va.gainDb + t * (vb.gainDb - va.gainDb);
    return true;
}

juce::String AdaptiveRecording::toBase64() const
{
    auto* d = data.load (std::memory_order_acquire);
    const int frames = numFrames.load();
    if (d == nullptr || frames == 0)
        return {};

    const juce::ScopedLock sl (cacheLock);
    const auto rev = revision.load (std::memory_order_relaxed);   // vor dem Lesen: spätere Änderungen packen neu
    if (rev == cachedRevision && cachedBase64.isNotEmpty())
        return cachedBase64;

    juce::MemoryOutputStream raw;
    raw.writeInt (frames);
    for (size_t i = 0; i < (size_t) frames * kValues; ++i)
        raw.writeShort (d[i].load (std::memory_order_relaxed));   // little endian

    juce::MemoryOutputStream zipped;
    {
        juce::GZIPCompressorOutputStream gz (zipped, 9);
        gz.write (raw.getData(), raw.getDataSize());
        gz.flush();
    }
    cachedBase64 = zipped.getMemoryBlock().toBase64Encoding();
    cachedRevision = rev;
    return cachedBase64;
}

bool AdaptiveRecording::fromBase64 (const juce::String& s)
{
    juce::MemoryBlock zipped;
    if (s.isEmpty() || ! zipped.fromBase64Encoding (s))
        return false;

    juce::MemoryInputStream zin (zipped, false);
    juce::GZIPDecompressorInputStream gz (zin);
    juce::MemoryBlock raw;
    gz.readIntoMemoryBlock (raw);

    juce::MemoryInputStream in (raw, false);
    const int frames = in.readInt();
    if (frames <= 0 || frames > kMaxFrames || (int64_t) raw.getSize() < 4 + (int64_t) frames * kValues * 2)
        return false;

    allocate();
    clear();
    auto* d = data.load();
    for (size_t i = 0; i < (size_t) frames * kValues; ++i)
        d[i].store ((int16_t) in.readShort(), std::memory_order_relaxed);
    numFrames.store (frames);
    revision.fetch_add (1, std::memory_order_relaxed);
    return true;
}
} // namespace ame
