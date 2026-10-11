// Exercise the real texture-cache functions with the iOS fault-free configuration.
#include "rend/TexCache.h"
#include "hw/mem/_vmem.h"
#include "hw/pvr/pvr_mem.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "deps/xxhash/xxhash.h"
unsigned VRAM_SIZE = 8 * 1024 * 1024;
unsigned VRAM_MASK = VRAM_SIZE - 1;
u32 FrameCount = 1;
void os_FatalError(const char*, const char*, int) { std::abort(); }
void GenericLog(LogTypes::LOG_LEVELS, LogTypes::LOG_TYPE, const char*, int, const char*, ...) { std::abort(); }
static unsigned protects = 0, unprotects = 0;
void _vmem_unprotect_vram(u32, u32) { ++unprotects; }
void _vmem_protect_vram(u32, u32) { ++protects; }
struct Texture : BaseTextureCacheData {
    std::string GetId() override { return "fixture"; }
    void UploadToGPU(int, int, u8*, bool, bool) override {}
    bool Delete() override { return true; }
};
int main() {
    vram.data = static_cast<u8*>(std::calloc(VRAM_SIZE, 1));
    vram.size = VRAM_SIZE;
    Texture t{};
    t.tcw.full = 0;
    t.tcw.PixelFmt = Pixel565;
    t.tex_type = TextureType::_565;
    t.sa = t.sa_tex = 4096; // A 4KiB texture page need not align to iOS's host page.
    t.size = 256;
    t.lock_block = nullptr;
    t.dirty = 0;
#if defined(TARGET_NO_EXCEPTIONS)
    for (u32 updates : {0u, 1u, 7u, 8u, 20u}) {
        t.Updates = updates;
        libCore_vramlock_Lock(t.sa_tex, t.sa + t.size - 1, &t);
        assert(t.lock_block == nullptr);
        assert(!t.NeedsUpdate());
        vram[t.sa + 17] ^= 1;
        assert(t.NeedsUpdate());
        libCore_vramlock_Lock(t.sa_tex, t.sa + t.size - 1, &t);
        assert(!t.NeedsUpdate());
        vram[t.sa + t.size + 1] ^= 1;
        assert(!t.NeedsUpdate());
    }
    t.tcw.PixelFmt = PixelPal4;
    t.tcw.PalSelect = 2;
    t.palette_hash = pal_hash_16[2];
    assert(!t.NeedsUpdate());
    pal_hash_16[2] ^= 1;
    assert(t.NeedsUpdate());
    t.tcw.PixelFmt = Pixel565;
    t.sa = t.sa_tex = VRAM_SIZE - 32;
    t.size = 256;
    libCore_vramlock_Lock(t.sa_tex, VRAM_SIZE - 1, &t);
    assert(!t.NeedsUpdate());
    vram[VRAM_SIZE - 1] ^= 1;
    assert(t.NeedsUpdate());
    assert(protects == 0 && unprotects == 0);
    puts("PASS: fault-free first-use/repeated changes, unchanged contents, palette changes, VRAM bounds; no page protection calls.");
#else
    t.Updates = 1;
    libCore_vramlock_Lock(t.sa_tex, t.sa + t.size - 1, &t);
    assert(t.lock_block != nullptr && protects == 1);
    assert(!t.NeedsUpdate());
    vram[t.sa + 17] ^= 1;
    assert(VramLockedWriteOffset(t.sa + 17));
    assert(t.NeedsUpdate());
    assert(t.lock_block == nullptr && unprotects == 1);
    t.dirty = 0;
    t.Updates = FREQUENT_UPDATE_COUNT;
    t.vram_hash = t.ComputeVramHash();
    assert(!t.NeedsUpdate());
    vram[t.sa + 17] ^= 1;
    assert(t.NeedsUpdate());
    puts("PASS: desktop protection, fault invalidation, and frequent-update hash behavior preserved.");
#endif
    std::free(vram.data);
}
