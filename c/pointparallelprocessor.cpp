#include <algorithm>    // std::min

#include "pointparallelprocessor.hpp"
#include <unistd.h>

template <typename Curve>
PointParallelProcessor<Curve>::PointParallelProcessor(Curve &_curve, uint32_t nThreads, typename Curve::PointAffine *_bases)  : curve(_curve) {
    bases = _bases;
    terminated = false;
    currentLevel = 0;
    nOpsExecuted = 0;
    nOpsCommited = 0;
    nOpsExecuting = 0;
//    ops.reserve(MAX_LEVELS+1);

    for (int i=0; i<nThreads; i++) {
        threads.push_back(std::thread(&PointParallelProcessor<Curve>::childThread, this));
    }
}

template <typename Curve>
PointParallelProcessor<Curve>::~PointParallelProcessor() {
}


template <typename Curve>
void PointParallelProcessor<Curve>::childThread() {
    uint32_t l;
    uint64_t start;
    uint64_t end;
    
    nice(5);
    while(1) {
        std::unique_lock<std::mutex> lk(mutex);
        start=0;
        end=0;
        if (terminated) {
            if (currentLevel == nLevels) return;
            while (ops[currentLevel].size() == nOpsExecuted) {
                currentLevel ++;
                nOpsExecuted = 0;
                nOpsExecuting = 0;
                if (currentLevel == nLevels) return;
            }
            l = currentLevel;
            start = nOpsExecuting;
            end = std::min(nOpsExecuting + NOPS_CHUNK, (uint64_t)ops[currentLevel].size());
        } else {
            if (nOpsCommited > nOpsExecuting) {
                l = currentLevel;
                start = nOpsExecuting;
                end = std::min(nOpsExecuting + NOPS_CHUNK, nOpsCommited);
            }
        }
        if (start != end) {
            nOpsExecuting = end;
            lk.unlock();
            innerProcess(l, start, end);
        } else {
            cv.wait(lk);
        }
    }
}

template <typename Curve>
void PointParallelProcessor<Curve>::innerProcess(uint32_t level, uint64_t start, uint64_t end) {
    for (uint64_t i=start; i<end; i++) {
        Op &op=ops[level][i];
        switch (op.fn) {
            case ADD: curve.add( *(typename Curve::Point *)(op.r), *(typename Curve::Point *)(op.a), *(typename Curve::Point *)(op.b) ); break;
            case ADD_MIXED: curve.add( *(typename Curve::Point *)(op.r), *(typename Curve::Point *)(op.a), *(typename Curve::PointAffine *)(op.b) ); break;
            case ADD_AFFINE: curve.add( *(typename Curve::Point *)(op.r), *(typename Curve::PointAffine *)(op.a), *(typename Curve::PointAffine *)(op.b) ); break;
        }
    }
    std::unique_lock<std::mutex> lk(mutex);
    nOpsExecuted += (end - start);
    cv.notify_all();
}


template <typename Curve>
typename PointParallelProcessor<Curve>::Point PointParallelProcessor<Curve>::add(Point a, Point b) {
    if (terminated) throw new std::runtime_error("Parallel Processor already terminated");
    if (a.source == ZERO) return b;
    if (b.source == ZERO) return a;
    uint32_t level = std::max(a.level, b.level);
    Point r = allocHeapPoint(level+1);
    if (a.source == BASE) {
        if (b.source == BASE) {
            addOp(level, ADD_AFFINE, r, a, b);
        } else {
            addOp(level, ADD_MIXED,  r, b, a);
        }
    } else {
        if (b.source == BASE) {
            addOp(level, ADD_MIXED, r, a, b);
        } else {
            addOp(level, ADD, r, a, b);
        }
    }
    return r;
}

template <typename Curve>
void PointParallelProcessor<Curve>::addOp(uint32_t level, Function fn, Point r, Point a, Point b) {
    if (level>MAX_LEVELS) {
        throw std::range_error("Max levels reacheed");
    }
/*    while (ops.size() < level + 1) {
        GrowableArray<Op> arr;
        ops.push_back(arr);
    }
*/    
    if (nLevels<=level) nLevels = level+1;
    ops[level].push_back(Op(
        fn,
        getPointPointer(r),
        getPointPointer(a),
        getPointPointer(b)
    ));

    if (ops[0].size() - nOpsCommited > NOPS_CHUNK ) {
        std::unique_lock<std::mutex> lk(mutex);
        nOpsCommited = ops[0].size();
        lk.unlock();
        cv.notify_all();
    }
}

template <typename Curve>
typename PointParallelProcessor<Curve>::Point PointParallelProcessor<Curve>::allocHeapPoint(uint32_t level) {
    Point p(
        HEAP,
        level,
        heap.size()
    );
    heap.push_back();
    return p;
}

template <typename Curve>
void *PointParallelProcessor<Curve>::getPointPointer(Point p) {
    switch (p.source) {
        case ZERO: return (void *)&curve.zero();
        case BASE: return (void *)&bases[p.idx];
        case HEAP: return (void *)&heap[p.idx];
    }
    throw std::invalid_argument("Point has invalid source");
}

template <typename Curve>
void PointParallelProcessor<Curve>:: extractResult(typename Curve::Point &r, Point p) {
    switch (p.source) {
        case ZERO: curve.copy(r, curve.zero()); return;
        case BASE: curve.copy(r, bases[p.idx]); return;
        case HEAP: curve.copy(r, heap[p.idx]); return;
    }
    throw std::invalid_argument("Point has invalid source");
}

template <typename Curve>
void PointParallelProcessor<Curve>::terminateCalculus() {
    std::unique_lock<std::mutex> lk(mutex);
    nOpsCommited = ops[0].size();
    terminated = true;
    lk.unlock();
    cv.notify_all();

    for (int i=0; i<threads.size(); i++) {
        threads[i].join();
    }
}
