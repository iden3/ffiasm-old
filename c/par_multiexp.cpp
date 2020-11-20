#include <omp.h>
#include "misc.hpp"

template <typename Curve>
void ParallelMultiexp<Curve>::packThreadsInner(uint32_t idThread, uint32_t start, uint32_t end) {
    for (uint32_t i=start; i<end; i++) {
        for (uint32_t j=1; j<nThreads; j++) {
            if (accs[j*accsPerChunk+i]) {
                accs[i] = addTrees(idThread, accs[i], accs[j*accsPerChunk+i]);
            }
        }
    }
}

template <typename Curve>
void ParallelMultiexp<Curve>::packThreads() {
    uint32_t n = 1 << bitsPerChunk;
    u_int64_t increment = n / nThreads;
    std::vector<std::thread> threads(nThreads-1);
    if (increment) {
        for (u_int64_t i=0; i<nThreads-1; i++) {
            threads[i] = std::thread (&ParallelMultiexp<Curve>::packThreadsInner, this, i, i*increment, (i+1)*increment);
        }
    }
    packThreadsInner(nThreads-1, (nThreads-1)*increment, n);
    if (increment) {
        for (u_int32_t i=0; i<nThreads-1; i++) {
            if (threads[i].joinable()) threads[i].join();
        }
    }
}

template <typename Curve>
void ParallelMultiexp<Curve>::reduceInner(uint32_t idThread, uint32_t nBits, uint32_t start, uint32_t end) {
    uint32_t h = 1 << (nBits -1);
    Point p;
    for (uint32_t i = start == 0 ? 1 : start; i<end; i++) {
        p = getPoint(idThread, accs[h+i]);
        accs[i] = addPoint(idThread, accs[i], p);
        accs[idThread*accsPerChunk + h] = addPoint(idThread, accs[idThread*accsPerChunk + h], p);
    }
}

template <typename Curve>
typename ParallelMultiexp<Curve>::Point ParallelMultiexp<Curve>::reduce(uint32_t nBits) {
    if (nBits==1) return getPoint(0, accs[1]);
    uint32_t ndiv2 = 1 << (nBits-1);

    for (u_int32_t i=1; i<nThreads; i++) {
        accs[i*accsPerChunk + ndiv2] = NULL;
    }

    u_int64_t increment = ndiv2 / nThreads;
    std::vector<std::thread> threads(nThreads-1);
    if (increment) {
        for (u_int64_t i=0; i<nThreads-1; i++) {
            threads[i] = std::thread (&ParallelMultiexp<Curve>::reduceInner, this, i, nBits, i*increment, (i+1)*increment);
        }
    }
    reduceInner(nThreads-1, nBits, (nThreads-1)*increment, ndiv2);
    if (increment) {
        for (u_int32_t i=0; i<nThreads-1; i++) {
            if (threads[i].joinable()) threads[i].join();
        }
    }
    Point p1 = reduce(nBits-1);
    for (u_int32_t i=1; i<nThreads; i++) {
        accs[ndiv2] = addTrees(0, accs[ndiv2], accs[i*accsPerChunk+ndiv2]);
    }
    Point p2 = getPoint(0, accs[ndiv2]);
    for (u_int32_t i=0; i<nBits-1; i++) p2 = prc->add(0, p2, p2);
    p1 = prc->add(0, p1, p2);
    return p1;
}

template <typename Curve>
void ParallelMultiexp<Curve>::initTrees() {
    treePages = new TreePageList[nThreads];
    freeTrees = new TreeList[nThreads];

    #pragma omp parallel for
    for (int i=0; i<nThreads; i++) {
        treePages[i].pages=NULL;
        freeTrees[i].trees=NULL;
        for (uint32_t j=0; j<accsPerChunk; j++) {
            accs[i*accsPerChunk+j]=NULL;
        }
    }
}

template <typename Curve>
void ParallelMultiexp<Curve>::uninitTrees() {
    #pragma omp parallel for
    for (uint32_t i=0; i< nThreads; i++) {
        while (treePages[i].pages!=NULL) {
            TreePage *oldTreePage = treePages[i].pages;
            treePages[i].pages = treePages[i].pages->next;
            delete[] oldTreePage->trees;
            delete oldTreePage;
        }
    }
    delete[] treePages;
    delete[] freeTrees;
}

template <typename Curve>
void ParallelMultiexp<Curve>::allocPage(uint32_t idThread) {
    TreePage *p = new TreePage;
    p->trees = new Tree[TREES_PER_PAGE];
    p->nUsed = 0;
    p->next = treePages[idThread].pages;
    treePages[idThread].pages = p;
}

template <typename Curve>
typename ParallelMultiexp<Curve>::Tree *ParallelMultiexp<Curve>::allocTree(uint32_t idThread, Point p, Tree *next, uint32_t nLevels) {
    Tree *r = freeTrees[idThread].trees;
    if (!r) {
        if ((!treePages[idThread].pages)||(treePages[idThread].pages->nUsed == TREES_PER_PAGE )) {
            allocPage(idThread);
        }
        r = &treePages[idThread].pages->trees[treePages[idThread].pages->nUsed++ ];
    } else {
        freeTrees[idThread].trees = r->next;
    }
    r->p = p;
    r->next = next;
    r->nLevels = nLevels;
    return r;
}

template <typename Curve>
void ParallelMultiexp<Curve>::freeTree(uint32_t idThread, Tree *t) {
    t->next = freeTrees[idThread].trees;
    freeTrees[idThread].trees = t;
}        

template <typename Curve>
typename ParallelMultiexp<Curve>::Tree *ParallelMultiexp<Curve>::addPoint(uint32_t idThread, Tree *curTree, Point p) {
    Tree *t = allocTree(idThread, p, curTree, 1);
    while ((t->next)&&(t->nLevels == t->next->nLevels)) {
        Tree *oldNext = t->next;

        t->p = prc->add(idThread, t->p, t->next->p);
        t->nLevels ++;
        t->next = t->next->next;
        freeTree(idThread, oldNext);
    }
    return t;
}

template <typename Curve>
typename ParallelMultiexp<Curve>::Tree *ParallelMultiexp<Curve>::addTrees(uint32_t idThread, Tree *curTree, Tree *other) {
    Tree *r = curTree;
    Tree *t = other;
    while (t) {
        r = addPoint(idThread, r, t->p);
        t = t->next;
    }
    return r;
}

template <typename Curve>
typename ParallelMultiexp<Curve>::Point ParallelMultiexp<Curve>::getPoint(uint32_t idThread, Tree *curTree) {
    Point r = prc->zero();
    Tree *t = curTree;
    while(t) {
        r = prc->add(idThread, r, t->p);
        t = t->next;
    }
    return r;
}

template <typename Curve>
uint32_t ParallelMultiexp<Curve>::getChunk(uint32_t scalarIdx, uint32_t chunkIdx) {
    uint32_t bitStart = chunkIdx*bitsPerChunk;
    uint32_t byteStart = bitStart/8;
    uint32_t efectiveBitsPerChunk = bitsPerChunk;
    if (byteStart > scalarSize-8) byteStart = scalarSize - 8;
    if (bitStart + bitsPerChunk > scalarSize*8) efectiveBitsPerChunk = scalarSize*8 - bitStart;
    uint32_t shift = bitStart - byteStart*8;
    uint64_t v = *(uint64_t *)(scalars + scalarIdx*scalarSize + byteStart);
    v = v >> shift;
    v = v & ( (1 << efectiveBitsPerChunk) - 1);
    return uint32_t(v);
}

/*
template <typename Curve>
void ParallelMultiexp<Curve>::processChunkInner(uint32_t idThread, uint32_t idChunk, uint32_t start, uint32_t end) {
    for(uint32_t i=start; i<end; i++) {
        if (g.isZero(bases[i])) continue;
        uint32_t chunkValue = getChunk(i, idChunk);
        if (chunkValue) {
            Tree *t = addPoint(idThread, accs[idThread*accsPerChunk+chunkValue], prc->basePoint(i));
            accs[idThread*accsPerChunk+chunkValue] = t;
        }
    }
}

template <typename Curve>
void ParallelMultiexp<Curve>::processChunk(uint32_t idChunk) {
    u_int64_t increment = n / nThreads;
    std::vector<std::thread> threads(nThreads-1);
    if (increment) {
        for (u_int64_t i=0; i<nThreads-1; i++) {
            threads[i] = std::thread (&ParallelMultiexp<Curve>::processChunkInner, this, i, idChunk, i*increment, (i+1)*increment);
        }
    }
    processChunkInner(nThreads-1, idChunk, (nThreads-1)*increment, n);
    if (increment) {
        for (u_int32_t i=0; i<nThreads-1; i++) {
            if (threads[i].joinable()) threads[i].join();
        }
    }
}
*/

template <typename Curve>
void ParallelMultiexp<Curve>::processChunk(uint32_t idChunk) {
    #pragma omp parallel for
    for(uint32_t i=0; i<n; i++) {
        int idThread = omp_get_thread_num();
        if (g.isZero(bases[i])) continue;
        uint32_t chunkValue = getChunk(i, idChunk);
        if (chunkValue) {
            accs[idThread*accsPerChunk+chunkValue] = addPoint(idThread, accs[idThread*accsPerChunk+chunkValue], prc->basePoint(i));
        }
    }
}


template <typename Curve>
void ParallelMultiexp<Curve>::multiexp(typename Curve::Point &r, typename Curve::PointAffine *_bases, uint8_t* _scalars, uint32_t _scalarSize, uint32_t _n, uint32_t _nThreads) {
    nThreads = _nThreads==0 ? std::thread::hardware_concurrency() : _nThreads;
//    nThreads = 1;
    bases = _bases;
    scalars = _scalars;
    scalarSize = _scalarSize;
    n = _n;


    if (n==0) {
        g.copy(r, g.zero());
        return;
    }
    if (n==1) {
        g.mulByScalar(r, bases[0], scalars, scalarSize);
        return;
    }
    bitsPerChunk = log2(n / PME_PACK_FACTOR);
    if (bitsPerChunk > PME_MAX_CHUNK_SIZE_BITS) bitsPerChunk = PME_MAX_CHUNK_SIZE_BITS;
    if (bitsPerChunk < PME_MIN_CHUNK_SIZE_BITS) bitsPerChunk = PME_MIN_CHUNK_SIZE_BITS;
    nChunks = ((scalarSize*8 - 1 ) / bitsPerChunk)+1;
    accsPerChunk = 1 << bitsPerChunk;  // In the chunks last bit is always zero.

    typename Curve::Point *chunkResults = new typename Curve::Point[nChunks];
    accs = new Tree *[nThreads*accsPerChunk];

    for (uint32_t i=0; i<nChunks; i++) {
        std::cout << "Allocating: " << i << "\n"; 
        prc = new PointParallelProcessor<Curve>(g, nThreads, bases);
        std::cout << "InitTrees " << i << "\n"; 
        initTrees();
        if (i==0) {
            for (int j=0; j<nThreads; j++) allocPage(j);
        }
        std::cout << "process chunks " << i << "\n"; 
        processChunk(i);
        std::cout << "pack " << i << "\n"; 
        packThreads();
        std::cout << "reduce " << i << "\n"; 
        Point p = reduce(bitsPerChunk);
        std::cout << "calculate " << i << "\n"; 
        prc->calculate();
        prc->extractResult(chunkResults[i], p);
        std::cout << "uninit " << i << "\n"; 
        uninitTrees();
        delete prc;
    }

    delete[] accs;

    g.copy(r, chunkResults[nChunks-1]);
    for  (int j=nChunks-2; j>=0; j--) {
        for (uint32_t k=0; k<bitsPerChunk; k++) g.dbl(r,r);
        g.add(r, r, chunkResults[j]);
    }

    delete[] chunkResults; 
}
