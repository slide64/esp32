#pragma once
#include <cstdint>
#include <cstring>
struct WavInfo { uint32_t offset = 0, bytes = 0; };
inline uint16_t le16(const uint8_t *p) { return p[0] | (uint16_t(p[1]) << 8); }
inline uint32_t le32(const uint8_t *p) { return le16(p) | (uint32_t(le16(p+2)) << 16); }
// Reader: size(), seek(offset), read(uint8_t*, count). PCM mono 24 kHz, 16 bits only.
template<class Reader> bool parseWav(Reader &f, WavInfo &out) {
  uint8_t h[16];
  if (!f.seek(0) || f.read(h, 12) != 12 || memcmp(h,"RIFF",4) || memcmp(h+8,"WAVE",4)) return false;
  uint64_t end = uint64_t(le32(h+4)) + 8;
  if (end > f.size() || end < 12) return false;
  bool format = false;
  uint64_t pos = 12;
  while (pos + 8 <= end) {
    if (!f.seek(pos) || f.read(h,8) != 8) return false;
    uint32_t n = le32(h+4); pos += 8;
    if (n > end-pos) return false;
    if (!memcmp(h,"fmt ",4)) {
      if (n < 16 || f.read(h,16) != 16) return false;
      format = le16(h)==1 && le16(h+2)==1 && le32(h+4)==24000 &&
               le32(h+8)==48000 && le16(h+12)==2 && le16(h+14)==16;
      if (!format) return false;
    } else if (!memcmp(h,"data",4)) {
      if (!format || !n || n%2) return false;
      out.offset=uint32_t(pos); out.bytes=n; return f.seek(out.offset);
    }
    pos += uint64_t(n) + (n & 1);
  }
  return false;
}
