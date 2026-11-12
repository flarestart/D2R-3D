// Included in d2r3d.cpp's anonymous namespace.
struct MapStamp { automap::Transform transform; automap::Point pivot; };
thread_local MapStamp* t_mapStamp = nullptr;
using MapMarks = std::unordered_map<size_t, MapStamp>;
SRWLOCK g_mapQueueLock = SRWLOCK_INIT;
std::unordered_map<uintptr_t, MapMarks> g_mapQueues;
thread_local const MapMarks* t_renderMarks = nullptr;
thread_local uintptr_t t_renderBase = 0;
thread_local bool t_captureVertices = false;
thread_local automap::Vertex* t_vertices = nullptr;
std::atomic<bool> g_mapReady{false};

struct MapScope {
    MapStamp stamp{};
    MapStamp* previous = t_mapStamp;
    explicit MapScope(uintptr_t view) {
        if (!g_mapReady.load() || !g_enabled.load() || !view) return;
        AcquireSRWLockShared(&g_camLock);
        stamp.transform = g_mapTransform;
        ReleaseSRWLockShared(&g_camLock);
        stamp.pivot = {(float)F<int32_t>(view, 0x28), (float)F<int32_t>(view, 0x2C)};
        t_mapStamp = &stamp;
    }
    ~MapScope() { t_mapStamp = previous; }
};
using MapListFn = void (*)(int, uintptr_t, uintptr_t, uintptr_t);
using MapUnitFn = void (*)(uintptr_t, uintptr_t);
using AppendFn = uintptr_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
using RenderFn = void (*)(uintptr_t, uintptr_t, uintptr_t);
using QuadFn = void (*)(uintptr_t, uintptr_t, uintptr_t, uint32_t, uint32_t);
using AllocateFn = uintptr_t (*)(uintptr_t, uintptr_t, uint32_t, uint32_t);
MapListFn OrigMapList = nullptr;
MapUnitFn OrigMapUnit = nullptr;
AppendFn OrigMapAppend = nullptr;
RenderFn OrigMapRender = nullptr;
QuadFn OrigMapQuad = nullptr;
AllocateFn OrigMapAllocate = nullptr;
void HookMapList(int tiles, uintptr_t list, uintptr_t view, uintptr_t opacity) {
    MapScope scope(view);
    OrigMapList(tiles, list, view, opacity);
}
void HookMapUnit(uintptr_t unit, uintptr_t view) {
    MapScope scope(view);
    OrigMapUnit(unit, view);
}
uintptr_t HookMapAppend(uintptr_t queue, uintptr_t pos, uintptr_t first, uintptr_t last) {
    // Store byte offsets, never pointers into a vector that may reallocate.
    const size_t offset = pos - F<uintptr_t>(queue, 0);
    const uintptr_t result = OrigMapAppend(queue, pos, first, last);
    AcquireSRWLockExclusive(&g_mapQueueLock);
    auto it = g_mapQueues.find(queue);
    if (offset == 0 && it != g_mapQueues.end()) it->second.clear();
    if (t_mapStamp && last > first && last-first >= 0x60 && F<uint32_t>(first, 0) == 0)
        g_mapQueues[queue][offset] = *t_mapStamp;
    ReleaseSRWLockExclusive(&g_mapQueueLock);
    return result;
}
void HookMapRender(uintptr_t renderer, uintptr_t queue, uintptr_t context) {
    MapMarks marks;
    AcquireSRWLockExclusive(&g_mapQueueLock);
    auto it = g_mapQueues.find(queue);
    if (it != g_mapQueues.end()) { marks.swap(it->second); g_mapQueues.erase(it); }
    ReleaseSRWLockExclusive(&g_mapQueueLock);
    const auto* previous = t_renderMarks;
    const auto previousBase = t_renderBase;
    t_renderMarks = &marks; t_renderBase = F<uintptr_t>(queue, 0);
    OrigMapRender(renderer, queue, context);
    t_renderMarks = previous; t_renderBase = previousBase;
}
uintptr_t HookMapAllocate(uintptr_t allocator, uintptr_t out, uint32_t size, uint32_t alignment) {
    const auto result = OrigMapAllocate(allocator, out, size, alignment);
    if (t_captureVertices && size == sizeof(automap::Vertex)*4)
        t_vertices = (automap::Vertex*)F<uintptr_t>(out, 8);
    return result;
}
void HookMapQuad(uintptr_t renderer, uintptr_t command, uintptr_t context, uint32_t width, uint32_t height) {
    const MapStamp* stamp = nullptr;
    if (t_renderMarks && command >= t_renderBase) {
        auto it = t_renderMarks->find(command - t_renderBase);
        if (it != t_renderMarks->end()) stamp = &it->second;
    }
    const bool previousCapture = t_captureVertices;
    auto* previousVertices = t_vertices;
    t_captureVertices = stamp != nullptr; t_vertices = nullptr;
    OrigMapQuad(renderer, command, context, width, height);
    // The allocation remains mapped after binding; the draw is submitted by the caller.
    if (stamp && t_vertices)
        automap::TransformQuad(t_vertices, stamp->transform, stamp->pivot, (float)width, (float)height);
    t_captureVertices = previousCapture; t_vertices = previousVertices;
}

bool InstallAutomapHooks() {
    if (g_mapReady.load()) return true;
    // Some protected game routines are restored lazily on their first use.
    // Retry only while the feature is enabled, and rate-limit attempts/logs.
    static ULONGLONG lastAttempt = 0;
    const auto now = GetTickCount64();
    if (lastAttempt && now - lastAttempt < 2000) return false;
    lastAttempt = now;
    struct Site { const char* name; uintptr_t rva; uint8_t bytes[16]; void* hook; void** original; };
    Site sites[] = {
        {"MapList", 0xb6450, {0x48,0x89,0x5c,0x24,0x10,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57}, (void*)&HookMapList, (void**)&OrigMapList},
        {"MapUnit", 0xb72b0, {0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x40,0x48,0x8b,0xda,0x4c,0x8d,0x44}, (void*)&HookMapUnit, (void**)&OrigMapUnit},
        {"MapAppend", 0x51f8b0, {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48}, (void*)&HookMapAppend, (void**)&OrigMapAppend},
        {"MapRender", 0x94d050, {0x48,0x8b,0xc4,0x48,0x89,0x58,0x20,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56}, (void*)&HookMapRender, (void**)&OrigMapRender},
        {"MapQuad", 0x94dc70, {0x48,0x8b,0xc4,0x48,0x89,0x58,0x10,0x55,0x56,0x57,0x41,0x56,0x41,0x57,0x48,0x8d}, (void*)&HookMapQuad, (void**)&OrigMapQuad},
        {"MapAllocate", 0xa0af20, {0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57}, (void*)&HookMapAllocate, (void**)&OrigMapAllocate},
    };
    // Verify all sites before installing any of the optional map hooks.
    for (const auto& s : sites) {
        uint8_t live[16]{};
        if (!SafeRead(live, (void*)(g_base+s.rva), 16) || memcmp(live,s.bytes,16)) {
            static const char* lastMismatch = nullptr;
            if (lastMismatch != s.name) {
                Log("%s signature mismatch at %p; retry pending; bytes %02X %02X %02X %02X %02X %02X %02X %02X / %02X %02X %02X %02X %02X %02X %02X %02X",
                    s.name, (void*)(g_base+s.rva), live[0],live[1],live[2],live[3],live[4],live[5],live[6],live[7],
                    live[8],live[9],live[10],live[11],live[12],live[13],live[14],live[15]);
                lastMismatch = s.name;
            }
            return false;
        }
    }
    for (const auto& s : sites) {
        if (MH_CreateHook((void*)(g_base+s.rva), s.hook, s.original) != MH_OK) {
            for (const auto& cleanup : sites) MH_RemoveHook((void*)(g_base+cleanup.rva));
            Log("native map hook creation failed"); return false;
        }
    }
    for (const auto& s : sites) {
        if (MH_EnableHook((void*)(g_base+s.rva)) != MH_OK) {
            for (const auto& cleanup : sites) {
                MH_DisableHook((void*)(g_base+cleanup.rva));
                MH_RemoveHook((void*)(g_base+cleanup.rva));
            }
            Log("native map hook activation failed"); return false;
        }
    }
    g_mapReady.store(true);
    Log("native map rotation hooks installed");
    return true;
}
