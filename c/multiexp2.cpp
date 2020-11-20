#include "multiexp2.hpp"
#include "misc.hpp"

namespace MultByScalar {

uint32_t getChunk(uint8_t* scalars, uint32_t scalarSize,uint32_t bitsPerChunk, uint32_t scalarIdx, uint32_t chunkIdx) {
    uint32_t res;
    uint32_t lsbIdx = (chunkIdx*bitsPerChunk)/32;
    if (lsbIdx*4 >= scalarSize) return 0;
    uint32_t shiftBits = (chunkIdx*bitsPerChunk)%32;
    if (shiftBits) {
        res = (  *(uint32_t *)(scalars + scalarIdx*scalarSize + lsbIdx*4) ) >> shiftBits;
        res |= (  *(uint32_t *)(scalars + scalarIdx*scalarSize + (lsbIdx + 1)*4) ) << (32 - shiftBits);
    } else {
        res = (  *(uint32_t *)(scalars + scalarIdx*scalarSize + lsbIdx*4) );
    }
    uint32_t maskBits = (lsbIdx*32 + shiftBits + bitsPerChunk > scalarSize*8) ? (scalarSize*8 - (lsbIdx*32 + shiftBits)) : bitsPerChunk;
    res = res & ((1 << maskBits) - 1);
    return res;
}

} // namespace;