#ifndef PAR_MULTIEXP
#define PAR_MULTIEXP

#include "pointparallelprocessor.hpp"
#define PME_PACK_FACTOR 2
#define PME_MAX_CHUNK_SIZE_BITS 20
#define PME_MIN_CHUNK_SIZE_BITS 2
#define TREES_PER_PAGE 1<<16

template <typename Curve>
class ParallelMultiexp {

    typedef typename PointParallelProcessor<Curve>::Point Point;

    struct Tree {
        Point p;
        Tree *next;
        int nLevels;
    };

    struct TreeList {
        Tree *trees;
        uint8_t padding[128];
    };

    struct TreePage {
        Tree *trees;
        int nUsed;
        TreePage *next;
    };

    struct TreePageList {
        TreePage *pages;
        uint8_t padding[128];
    };


    Tree **accs;
    PointParallelProcessor<Curve> *prc;
    typename Curve::PointAffine *bases;
    uint8_t* scalars;
    uint32_t scalarSize;
    uint32_t n;
    uint32_t nThreads;
    uint32_t bitsPerChunk;
    uint64_t accsPerChunk;
    uint64_t accsPerChunkS;
    uint32_t nChunks;
    TreePageList *treePages;
    TreeList *freeTrees;
    Curve &g;

    void packThreadsInner(uint32_t idThread, uint32_t from, uint32_t to);
    void packThreads();
    void reduceInner(uint32_t idThread, uint32_t nBits, uint32_t from, uint32_t to);
    Point reduce(uint32_t nBits);
    void initTrees();
    void uninitTrees();
    void allocPage(uint32_t idThread);
    Tree *allocTree(uint32_t idThread, Point p, Tree *next, uint32_t nLevels);
    void freeTree(uint32_t idThread, Tree *t);
    Tree *addPoint(uint32_t idThread, Tree *curTree, Point p);
    void processChunkInner(uint32_t idThread, uint32_t idChunk, uint32_t from, uint32_t to);
    void processChunk(uint32_t idxChunk);
    Tree *addTrees(uint32_t idThread, Tree *curTree, Tree *other);
    Point getPoint(uint32_t idThread, Tree *curTree);
    uint32_t getChunk(uint32_t scalarIdx, uint32_t chunkIdx);

public:
    ParallelMultiexp(Curve &_g): g(_g) {}
    void multiexp(typename Curve::Point &r, typename Curve::PointAffine *_bases, uint8_t* _scalars, uint32_t _scalarSize, uint32_t _n, uint32_t _nThreads=0);

};

#include "par_multiexp.cpp"

#endif // PAR_MULTIEXP
