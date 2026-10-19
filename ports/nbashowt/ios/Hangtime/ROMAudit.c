#include "HangtimeCore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static unsigned le16(const unsigned char *p) { return p[0] | p[1]<<8; }
static unsigned le32(const unsigned char *p) { return le16(p) | le16(p+2)<<16; }
/* Validate payload CRCs as well as ZIP metadata. Arcade chips are small enough
   to audit in memory; never allocate according to untrusted archive sizes. */
bool ht_validate_rom(const char *path, char *report, size_t capacity) {
    FILE *f=fopen(path,"rb");
    if (!f) { snprintf(report,capacity,"Import nbashowt.zip (the arcade ROM chips)."); return false; }
    fseek(f,0,SEEK_END); long size=ftell(f); rewind(f);
    if (size<22 || size>4*1024*1024) { fclose(f); snprintf(report,capacity,"Invalid arcade ZIP archive."); return false; }
    unsigned char *data=malloc(size);
    if (!data) { fclose(f); return false; }
    bool ok=fread(data,1,size,f)==(size_t)size; fclose(f);
    bool found[2]={false,false};
    static const unsigned sizes[2]={0x80000,0x8000}, crcs[2]={0xff5d620d,0xd1470e23};
    long end=size-22;
    while (end>=0 && end>=size-65557 && le32(data+end)!=0x06054b50) --end;
    if (ok && end>=0 && end>=size-65557) {
        size_t at=le32(data+end+16);
        unsigned count=le16(data+end+10);
        for (unsigned i=0;i<count && i<1024 && at+46<=(size_t)size;++i) {
            const unsigned char *c=data+at;
            if (le32(c)!=0x02014b50) break;
            unsigned packed=le32(c+20), unpacked=le32(c+24), crc=le32(c+16), method=le16(c+10), offset=le32(c+42);
            for (unsigned j=0;j<2;++j) if (unpacked==sizes[j] && crc==crcs[j] && packed<=1024*1024 && (size_t)offset+30<=(size_t)size) {
                const unsigned char *l=data+offset;
                size_t payload=(size_t)offset+30+le16(l+26)+le16(l+28);
                if (le32(l)!=0x04034b50 || (le16(l+6)&1) || payload+packed>(size_t)size) continue;
                unsigned char *out=malloc(unpacked);
                if (!out) continue;
                bool valid=false;
                if (method==0 && packed==unpacked) { memcpy(out,data+payload,unpacked); valid=true; }
                if (method==8) {
                    z_stream z={0}; z.next_in=data+payload; z.avail_in=packed; z.next_out=out; z.avail_out=unpacked;
                    if (inflateInit2(&z,-MAX_WBITS)==Z_OK) { valid=inflate(&z,Z_FINISH)==Z_STREAM_END && z.total_out==unpacked; inflateEnd(&z); }
                }
                found[j]=valid && (unsigned)crc32(0,out,unpacked)==crc;
                free(out);
            }
            at+=46+le16(c+28)+le16(c+30)+le16(c+32);
        }
    }
    free(data);
    if (!found[0] || !found[1]) snprintf(report,capacity,"nbashowt.zip needs showtime_mar15_1999.u27 (ff5d620d) and vegassio.bin (d1470e23). PlayStation discs are not supported.");
    return found[0] && found[1];
}
bool ht_validate_disk(const char *path, char *report, size_t capacity) {
    static const unsigned char expected[20]={0xf7,0xc5,0x6b,0xc3,0xdc,0xbe,0xbc,0x43,0x4d,0xe5,0x80,0x34,0x98,0x61,0x79,0xae,0x01,0x12,0x7f,0x87};
    unsigned char h[124]; FILE *f=fopen(path,"rb");
    if (!f) { snprintf(report,capacity,"Import nbashowt.chd alongside nbashowt.zip. Both files are required for arcade Showtime."); return false; }
    size_t n=fread(h,1,sizeof h,f); fclose(f);
    /* CHD v5 combined SHA-1 identifies this disk; MAME checks block integrity. */
    bool ok=n==sizeof h && !memcmp(h,"MComprHD",8) && h[12]==0 && h[13]==0 && h[14]==0 && h[15]==5 && !memcmp(h+84,expected,20);
    if (!ok) snprintf(report,capacity,"Wrong or damaged disk header: expected the NBA Showtime 2.0 nbashowt.chd.");
    return ok;
}
