#include "wav.h"
#include <cassert>
#include <vector>
#include <algorithm>
struct Reader {
  std::vector<uint8_t> bytes; size_t pos=0;
  size_t size() { return bytes.size(); }
  bool seek(size_t n) { if(n>size())return false; pos=n; return true; }
  size_t read(uint8_t *p,size_t n) { n=std::min(n,size()-pos); memcpy(p,bytes.data()+pos,n);pos+=n;return n; }
};
void put32(std::vector<uint8_t> &v,size_t p,uint32_t x) { for(int i=0;i<4;++i)v[p+i]=(x>>(8*i))&255; }
Reader valid() {
  Reader f; f.bytes.resize(48);
  memcpy(f.bytes.data(),"RIFF",4);put32(f.bytes,4,40);memcpy(f.bytes.data()+8,"WAVEfmt ",8);
  put32(f.bytes,16,16); f.bytes[20]=1;f.bytes[22]=1;
  put32(f.bytes,24,24000);put32(f.bytes,28,48000);f.bytes[32]=2;f.bytes[34]=16;
  memcpy(f.bytes.data()+36,"data",4);put32(f.bytes,40,4);return f;
}
int main() {
  WavInfo w;
  auto f=valid();assert(parseWav(f,w));assert(w.offset==44 && w.bytes==4 && f.pos==44);
  f=valid();f.bytes.resize(46);assert(!parseWav(f,w)); // truncated data
  f=valid();f.bytes[22]=2;assert(!parseWav(f,w)); // stereo
  f=valid();put32(f.bytes,24,44100);assert(!parseWav(f,w)); // wrong clock
  f=valid();put32(f.bytes,40,0xfffffff0);assert(!parseWav(f,w)); // overflow attempt
  f=valid();f.bytes[20]=3;assert(!parseWav(f,w)); // floating point
  f=valid();put32(f.bytes,40,3);assert(!parseWav(f,w)); // odd sample count
  f=valid();put32(f.bytes,16,1);assert(!parseWav(f,w)); // malformed fmt
  f=valid();put32(f.bytes,4,0xffffffff);assert(!parseWav(f,w)); // corrupt RIFF
  f=valid();f.bytes[0]='X';assert(!parseWav(f,w));
  // Unknown metadata chunk before fmt must be skipped including padding.
  f=valid();f.bytes.insert(f.bytes.begin()+12,10,0);memcpy(f.bytes.data()+12,"JUNK",4);
  put32(f.bytes,16,1);put32(f.bytes,4,50);assert(parseWav(f,w));assert(w.offset==54);
}
