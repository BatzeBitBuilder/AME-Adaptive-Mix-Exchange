// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "MixHub.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <tlhelp32.h>
#else
 #include <cerrno>
 #include <csignal>
 #include <fcntl.h>
 #include <pthread.h>
 #include <sys/mman.h>
 #include <sys/stat.h>
 #include <unistd.h>
#endif

#include <map>
#include <new>
#include <set>

namespace mtm
{
void SlotAudioData::reset() noexcept
{
    rmsDb.store (-100.0f);
    peakDb.store (-100.0f);
    gainReductionDb.store (0.0f);
    gateReductionDb.store (0.0f); deessReductionDb.store (0.0f); limiterReductionDb.store (0.0f);
    dryPow.store (0.0f); wetPow.store (0.0f);
    for (auto& b : bandDb)             b.store (-100.0f);
    for (auto& b : bandAvgDb)          b.store (-100.0f);
    for (auto& d : displayDb)          d.store (-100.0f);
    rmsLeftDb.store (-100.0f);
    rmsRightDb.store (-100.0f);
    analysisCounter.store (0);
    blockCounter.store (0);
    priorityOffset.store (0.0f);
    reaction.store (0.3f);
    for (auto& v : targetCutDb)  v.store (0.0f);
    for (auto& v : appliedCutDb) v.store (0.0f);
    targetHpfHz.store (0.0f);  targetGainDb.store (0.0f);
    appliedHpfHz.store (0.0f); appliedGainDb.store (0.0f);
    frozen.store (false); bypassed.store (false);
    for (auto* f : { &fCentroid, &fFlatness, &fSub, &fLow, &fHigh, &fOnsetRate, &fHarmonicity, &fPitch, &fPitchVar, &fCrest, &fWidth, &fActiveSec })
        f->store (0.0f);
}

//==============================================================================
namespace
{
// Bei jeder Änderung von HubShared/SlotAudioData erhöhen → verschiedene AME-Versionen teilen keinen Speicher
constexpr uint32_t kLayoutVersion = 3;
constexpr uint32_t kMagic = 0x414d4548;   // "AMEH"

uint32_t currentPid() noexcept
{
   #if JUCE_WINDOWS
    return (uint32_t) GetCurrentProcessId();
   #else
    return (uint32_t) getpid();
   #endif
}

bool isProcessAlive (uint32_t pid) noexcept
{
    if (pid == 0)
        return false;
   #if JUCE_WINDOWS
    if (auto h = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD) pid))
    {
        DWORD code = 0;
        const bool alive = GetExitCodeProcess (h, &code) && code == STILL_ACTIVE;
        CloseHandle (h);
        return alive;
    }
    return GetLastError() == ERROR_ACCESS_DENIED;   // existiert, aber kein Zugriff
   #else
    return kill ((pid_t) pid, 0) == 0 || errno == EPERM;
   #endif
}

// Hilfsprozesse der DAWs (Plugin-Sandbox, Audio-Engine, Bridge) → Sitzung gehört zum Elternprozess
bool isHelperProcessName (juce::String exe)
{
    exe = exe.toLowerCase().removeCharacters (" -_");
    for (auto* key : { "pluginhost", "audioengine", "reaperhost", "bridge", "pluginbox", "pluginscanner" })
        if (exe.contains (key))
            return true;
    return false;
}

juce::String findSessionKey()
{
    auto env = juce::SystemStats::getEnvironmentVariable ("AME_HUB_SESSION", {}).trim();
    if (env.isNotEmpty())
        return env.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789").substring (0, 40);

    uint32_t pid = currentPid();

   #if JUCE_WINDOWS
    // Prozessbaum aufwärts, solange es Hilfsprozesse sind
    std::map<uint32_t, std::pair<uint32_t, juce::String>> procs;   // pid -> (parent, exe)
    if (auto snap = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0); snap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32W pe {};
        pe.dwSize = sizeof (pe);
        for (BOOL ok = Process32FirstW (snap, &pe); ok; ok = Process32NextW (snap, &pe))
            procs[(uint32_t) pe.th32ProcessID] = { (uint32_t) pe.th32ParentProcessID, juce::String (pe.szExeFile) };
        CloseHandle (snap);
    }

    for (int depth = 0; depth < 8; ++depth)
    {
        const auto it = procs.find (pid);
        if (it == procs.end() || ! isHelperProcessName (it->second.second))
            break;
        const auto parent = it->second.first;
        if (parent == 0 || procs.find (parent) == procs.end())
            break;
        pid = parent;
    }
   #endif

    return juce::String (pid);
}

// Kopf vor den Hub-Daten im gemeinsamen Speicher
struct HubHeader
{
    std::atomic<uint32_t> state;        // 0 = leer, 1 = wird angelegt, 2 = bereit
    uint32_t magic;
    uint32_t dataSize;
    std::atomic<int32_t> attachCount;
   #if ! JUCE_WINDOWS
    pthread_mutex_t mutex;
   #endif
};

constexpr size_t kHeaderSize = (sizeof (HubHeader) + 63) & ~(size_t) 63;
constexpr size_t kTotalSize = kHeaderSize + sizeof (HubShared);

void resetShared (HubShared& d) noexcept
{
    d.globalGenre.store (0);
    d.globalAmount.store (1.0f);
    d.globalBypass.store (false);
    d.freezeCommandGen.store (0);
    d.freezeCommandValue.store (false);
    d.engineHeartbeatMs.store (0);
    d.monitorMode.store (0);
    d.rawMatchDb.store (0.0f);
    d.nextInstanceId = 1;
    for (auto& i : d.infos) i = {};
    for (auto& a : d.audio) a.reset();
}
} // namespace

//==============================================================================
// Verbindung zum gemeinsamen Speicher (oder lokaler Fallback) + prozessübergreifende Sperre
class MixHub::Connection
{
public:
    Connection()
    {
        sessionKey = findSessionKey();
        baseName = "AME_Hub_v" + juce::String (kLayoutVersion) + "_" + juce::String ((int) sizeof (HubShared))
                     + "_" + sessionKey;

        if (! openShared())
        {
            closeShared();
            local = std::make_unique<HubShared>();
            resetShared (*local);
            data = local.get();
        }
    }

    ~Connection() { closeShared(); }

    HubShared& getData() noexcept { return *data; }
    bool isShared() const noexcept { return local == nullptr; }
    const juce::String& getSessionKey() const noexcept { return sessionKey; }

    // abgestürzter Prozess hat sich nicht abgemeldet → Zähler korrigieren (POSIX: Speicher sonst nie freigegeben)
    void noteProcessGone() noexcept
    {
        if (attached && header != nullptr)
            header->attachCount.fetch_sub (1);
    }

    void lock() noexcept
    {
        if (local != nullptr) { localLock.enter(); return; }
       #if JUCE_WINDOWS
        // Timeout statt Hänger, falls ein anderer Prozess blockiert; aufgegebene Sperre (Absturz) gilt als erhalten
        WaitForSingleObject (mutex, 2000);
       #else
        if (pthread_mutex_lock (&header->mutex) == EOWNERDEAD)
           #if JUCE_LINUX || JUCE_BSD
            pthread_mutex_consistent (&header->mutex);
           #else
            {}
           #endif
       #endif
    }

    void unlock() noexcept
    {
        if (local != nullptr) { localLock.exit(); return; }
       #if JUCE_WINDOWS
        ReleaseMutex (mutex);
       #else
        pthread_mutex_unlock (&header->mutex);
       #endif
    }

private:
    bool openShared()
    {
       #if JUCE_WINDOWS
        const auto mapName  = "Local\\" + baseName;
        const auto lockName = "Local\\" + baseName + "_lock";
        mutex = CreateMutexW (nullptr, FALSE, lockName.toWideCharPointer());
        if (mutex == nullptr)
            return false;

        mapping = CreateFileMappingW (INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                      (DWORD) ((uint64_t) kTotalSize >> 32), (DWORD) (kTotalSize & 0xffffffffu),
                                      mapName.toWideCharPointer());
        if (mapping == nullptr)
            return false;

        view = MapViewOfFile (mapping, FILE_MAP_ALL_ACCESS, 0, 0, kTotalSize);
        if (view == nullptr)
            return false;
       #else
        shmName = "/" + baseName;
        const int fd = shm_open (shmName.toRawUTF8(), O_CREAT | O_RDWR, 0600);
        if (fd < 0)
            return false;

        struct stat st {};
        const bool sizeOk = fstat (fd, &st) == 0
                            && (st.st_size == (off_t) kTotalSize || (st.st_size == 0 && ftruncate (fd, (off_t) kTotalSize) == 0));
        if (sizeOk)
            view = mmap (nullptr, kTotalSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        ::close (fd);

        if (! sizeOk || view == MAP_FAILED)
        {
            view = nullptr;
            return false;
        }
       #endif

        header = static_cast<HubHeader*> (view);
        auto* raw = static_cast<char*> (view) + kHeaderSize;

        // Erster Prozess legt an (Speicher ist vom System genullt), alle anderen warten auf "bereit"
        uint32_t expected = 0;
        if (header->state.compare_exchange_strong (expected, 1))
        {
           #if ! JUCE_WINDOWS
            pthread_mutexattr_t attr;
            pthread_mutexattr_init (&attr);
            pthread_mutexattr_setpshared (&attr, PTHREAD_PROCESS_SHARED);
           #if JUCE_LINUX || JUCE_BSD
            pthread_mutexattr_setrobust (&attr, PTHREAD_MUTEX_ROBUST);
           #endif
            pthread_mutex_init (&header->mutex, &attr);
            pthread_mutexattr_destroy (&attr);
           #endif
            header->magic = kMagic;
            header->dataSize = (uint32_t) sizeof (HubShared);
            auto* d = new (raw) HubShared();
            resetShared (*d);
            header->state.store (2, std::memory_order_release);
        }
        else
        {
            const auto t0 = juce::Time::getMillisecondCounter();
            while (header->state.load (std::memory_order_acquire) != 2)
            {
                if (juce::Time::getMillisecondCounter() - t0 > 1000)
                    return false;    // Anleger hängt/abgestürzt → lokal weiterarbeiten
                juce::Thread::sleep (1);
            }
        }

        if (header->magic != kMagic || header->dataSize != (uint32_t) sizeof (HubShared))
            return false;

        header->attachCount.fetch_add (1);
        attached = true;
        data = reinterpret_cast<HubShared*> (raw);
        return true;
    }

    void closeShared() noexcept
    {
        bool last = false;
        if (attached && header != nullptr)
            last = header->attachCount.fetch_sub (1) == 1;
        attached = false;

       #if JUCE_WINDOWS
        juce::ignoreUnused (last);   // Windows gibt den Speicher mit dem letzten Handle frei
        if (view != nullptr)    UnmapViewOfFile (view);
        if (mapping != nullptr) CloseHandle (mapping);
        if (mutex != nullptr)   CloseHandle (mutex);
        mapping = mutex = nullptr;
       #else
        if (view != nullptr)
            munmap (view, kTotalSize);
        if (last)
            shm_unlink (shmName.toRawUTF8());
       #endif
        view = nullptr;
        header = nullptr;
    }

    juce::String sessionKey, baseName;
    HubHeader* header = nullptr;
    void* view = nullptr;
    bool attached = false;
    HubShared* data = nullptr;
    std::unique_ptr<HubShared> local;
    juce::CriticalSection localLock;

   #if JUCE_WINDOWS
    HANDLE mapping = nullptr, mutex = nullptr;
   #else
    juce::String shmName;
   #endif
};

namespace
{
struct HubLock
{
    explicit HubLock (MixHub::Connection& c) : conn (c) { conn.lock(); }
    ~HubLock() { conn.unlock(); }
    MixHub::Connection& conn;
};
}

//==============================================================================
MixHub::MixHub()
    : conn (std::make_unique<Connection>()),
      shared (conn->getData()),
      ownPid (currentPid())
{
}

MixHub::~MixHub() = default;

bool MixHub::isShared() const noexcept       { return conn->isShared(); }
juce::String MixHub::getSessionKey() const   { return conn->getSessionKey(); }

void MixHub::purgeDeadOwners (bool force) const
{
    const auto now = juce::Time::getMillisecondCounter();
    if (! force && now - lastPurgeMs < 1000)
        return;
    lastPurgeMs = now;

    if (! conn->isShared())
        return;

    std::map<uint32_t, bool> alive;
    for (size_t i = 0; i < (size_t) kMaxSlots; ++i)
    {
        auto& info = shared.infos[i];
        if (! info.used || info.ownerPid == ownPid)
            continue;

        auto it = alive.find (info.ownerPid);
        if (it == alive.end())
            it = alive.emplace (info.ownerPid, isProcessAlive (info.ownerPid)).first;

        if (! it->second)     // Prozess beendet/abgestürzt, ohne abzumelden
        {
            info = {};
            shared.audio[i].reset();
        }
    }

    for (const auto& [pid, isAlive] : alive)
        if (! isAlive)
            conn->noteProcessGone();
}

int MixHub::registerInstance (uint32_t& outInstanceId)
{
    const HubLock hl (*conn);
    purgeDeadOwners (true);

    for (int i = 0; i < kMaxSlots; ++i)
    {
        auto& info = shared.infos[(size_t) i];
        if (! info.used)
        {
            info = {};
            info.used = true;
            info.instanceId = shared.nextInstanceId++;
            info.ownerPid = ownPid;
            ("Line " + juce::String (info.instanceId).paddedLeft ('0', 2)).copyToUTF8 (info.name, sizeof (info.name));
            shared.audio[(size_t) i].reset();
            outInstanceId = info.instanceId;
            return i;
        }
    }

    outInstanceId = 0;
    return -1;
}

void MixHub::unregisterInstance (int slot)
{
    if (! juce::isPositiveAndBelow (slot, kMaxSlots))
        return;

    const HubLock hl (*conn);
    auto& info = shared.infos[(size_t) slot];
    if (info.ownerPid != ownPid)   // Slot wurde inzwischen neu vergeben
        return;
    info = {};
    shared.audio[(size_t) slot].reset();
}

void MixHub::updateInfo (int slot, const juce::String& name, juce::Colour colour, Role role, InstanceMode mode,
                         bool autoRole, float autoConfidence, bool autoFromName)
{
    if (! juce::isPositiveAndBelow (slot, kMaxSlots))
        return;

    const HubLock hl (*conn);
    auto& info = shared.infos[(size_t) slot];
    if (! info.used || info.ownerPid != ownPid)
        return;

    if (name.isNotEmpty())
        name.copyToUTF8 (info.name, sizeof (info.name));

    info.colourArgb = colour.getARGB();
    info.role = (int32_t) role;
    info.mode = (int32_t) mode;
    info.autoRole = autoRole;
    info.autoConfidence = autoConfidence;
    info.autoFromName = autoFromName;
}

std::vector<TrackSnapshot> MixHub::getSnapshot() const
{
    std::vector<TrackSnapshot> result;
    result.reserve (16);
    const HubLock hl (*conn);
    purgeDeadOwners (false);

    for (int i = 0; i < kMaxSlots; ++i)
    {
        const auto& info = shared.infos[(size_t) i];
        if (! info.used)
            continue;

        const auto& a = shared.audio[(size_t) i];
        TrackSnapshot s;
        s.slot = i;
        s.instanceId = info.instanceId;
        s.name = juce::String::fromUTF8 (info.name, (int) strnlen (info.name, sizeof (info.name)));
        s.colour = juce::Colour (info.colourArgb);
        s.role = (Role) info.role;
        s.mode = (InstanceMode) info.mode;
        s.autoRole = info.autoRole;
        s.autoConfidence = info.autoConfidence;
        s.autoFromName = info.autoFromName;
        s.rmsDb = a.rmsDb.load();
        s.peakDb = a.peakDb.load();
        s.grDb = a.gainReductionDb.load();
        for (size_t b = 0; b < (size_t) kNumAnalysisBands; ++b)
        {
            s.bandDb[b] = a.bandDb[b].load (std::memory_order_relaxed);
            s.bandAvgDb[b] = a.bandAvgDb[b].load (std::memory_order_relaxed);
        }
        s.hasAnalysis = a.analysisCounter.load (std::memory_order_acquire) > 0;
        for (size_t d = 0; d < (size_t) kNumDisplayPoints; ++d)
            s.displayDb[d] = a.displayDb[d].load (std::memory_order_relaxed);
        s.rmsLeftDb = a.rmsLeftDb.load();
        s.rmsRightDb = a.rmsRightDb.load();
        for (size_t k = 0; k < (size_t) kNumAdaptiveBands; ++k)
            s.appliedCutDb[k] = a.appliedCutDb[k].load (std::memory_order_relaxed);
        s.appliedHpfHz = a.appliedHpfHz.load();
        s.appliedGainDb = a.appliedGainDb.load();
        s.priorityOffset = a.priorityOffset.load();
        s.reaction = a.reaction.load();
        s.frozen = a.frozen.load();
        s.bypassed = a.bypassed.load();
        result.push_back (std::move (s));
    }

    std::sort (result.begin(), result.end(),
               [] (const auto& a, const auto& b) { return a.instanceId < b.instanceId; });
    return result;
}

int MixHub::getNumInstances() const
{
    const HubLock hl (*conn);
    purgeDeadOwners (false);
    return (int) std::count_if (shared.infos.begin(), shared.infos.end(), [] (const auto& i) { return i.used; });
}

int MixHub::getNumMasters() const
{
    const HubLock hl (*conn);
    purgeDeadOwners (false);
    return (int) std::count_if (shared.infos.begin(), shared.infos.end(),
                                [] (const auto& i) { return i.used && i.mode == (int32_t) InstanceMode::Master; });
}

int MixHub::getNumProcesses() const
{
    const HubLock hl (*conn);
    std::set<uint32_t> pids;
    for (const auto& i : shared.infos)
        if (i.used)
            pids.insert (i.ownerPid);
    return (int) pids.size();
}

uint32_t MixHub::getActiveMasterId() const
{
    const HubLock hl (*conn);
    purgeDeadOwners (false);
    uint32_t best = 0;

    for (const auto& i : shared.infos)
        if (i.used && i.mode == (int32_t) InstanceMode::Master && (best == 0 || i.instanceId < best))
            best = i.instanceId;

    return best;
}

SlotAudioData* MixHub::getAudioData (int slot) noexcept
{
    return juce::isPositiveAndBelow (slot, kMaxSlots) ? &shared.audio[(size_t) slot] : nullptr;
}
} // namespace mtm
