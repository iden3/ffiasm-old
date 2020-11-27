#include <algorithm>    // std::min

#include "pointparallelprocessor.hpp"
#include <unistd.h>

template <typename Curve>
typename PointParallelProcessor<Curve>::Point PointParallelProcessor<Curve>::add(uint32_t idThread, Point a, Point b) {
    if (terminated) throw new std::runtime_error("Parallel Processor already terminated");
    if (a.source == ZERO) return b;
    if (b.source == ZERO) return a;
    uint32_t level = std::max(a.level, b.level);
    Point r = allocHeapPoint(idThread, level+1);
    if (a.source == BASE) {
        if (b.source == BASE) {
            addOp(idThread, level, ADD_AFFINE, r, a, b);
        } else {
            addOp(idThread, level, ADD_MIXED,  r, b, a);
        }
    } else {
        if (b.source == BASE) {
            addOp(idThread, level, ADD_MIXED, r, a, b);
        } else {
            addOp(idThread, level, ADD, r, a, b);
        }
    }
    return r;
}

template <typename Curve>
void PointParallelProcessor<Curve>::addOp(uint32_t idThread, uint32_t level, Function fn, Point r, Point a, Point b) {
    if (level>MAX_LEVELS) {
        throw std::range_error("Max levels reacheed");
    }

    if (nLevels<=level) nLevels = level+1;

    Op &op = ops[level]->addElement(idThread);
    op.fn = fn;
    op.r = r.p;
    op.a = a.p;
    op.b = b.p;
}

template <typename Curve>
typename PointParallelProcessor<Curve>::Point PointParallelProcessor<Curve>::allocHeapPoint(uint32_t idThread, uint32_t level) {
    Point p;
    p.source = HEAP;
    p.level = level;
    p.p = (void *)&heap->addElement(idThread);
    return p;
}

template <typename Curve>
void PointParallelProcessor<Curve>:: extractResult(typename Curve::Point &r, Point &p) {
    switch (p.source) {
        case ZERO: curve.copy(r, curve.zero()); return;
        case BASE: curve.copy(r, *(typename Curve::PointAffine *)p.p); return;
        case HEAP: curve.copy(r, *(typename Curve::Point *)p.p); return;
    }
    throw std::invalid_argument("Point has invalid source");
}

template <typename Curve>
void PointParallelProcessor<Curve>::calculate() {
    terminated = true;
    currentLevel = 0;
    pendingThreads = 0;
    itExecuting = ops[currentLevel]->begin();

    for (uint32_t i=0; i<nThreads; i++) {
        threads.push_back(std::thread(&PointParallelProcessor<Curve>::childThread, this, i));
    }

    cv.notify_all();

    for (uint32_t i=0; i<threads.size(); i++) {
        threads[i].join();
    }
}


template <typename Curve>
void PointParallelProcessor<Curve>::childThread(uint32_t th) {
    uint32_t l;
    
    // nice(5);
    while(1) {
        std::unique_lock<std::mutex> lk(cv_mutex);
//        std::cout << "Checking: " << th << "\n"; 
        if (currentLevel == nLevels) {
            cv.notify_all();
            return;
        }
        while ((itExecuting == ops[currentLevel]->end())&&(pendingThreads  == 0)) {
            currentLevel ++;
//            std::cout << "----==============================================--->New Level: " << currentLevel << " th: " << th << "\n"; 
            if (currentLevel == nLevels) {
                cv.notify_all();
                return;
            }
            itExecuting = ops[currentLevel]->begin();
        }
        l = currentLevel;
        typename GrowableArrayMT<Op>::Iterator start = itExecuting;
        typename GrowableArrayMT<Op>::Iterator end = itExecuting + NOPS_CHUNK;
        if (start != end) {
            itExecuting = end;
            pendingThreads ++;
            lk.unlock();
            cv.notify_one();
//            std::cout << "Start Inner: " << th << "\n"; 
            innerProcess(l, start, end);
//            std::cout << "End Inner: " << th << "\n"; 
            lk.lock();
//            std::cout << "End Inner After lock: " << th << "\n"; 
            pendingThreads--;
        } else {
//            std::cout << "Nothing to do: " << th << "\n"; 
            cv.wait(lk);
        }
    }
}

template <typename Curve>
void PointParallelProcessor<Curve>::innerProcess(uint32_t level, typename GrowableArrayMT<Op>::Iterator start, typename GrowableArrayMT<Op>::Iterator end) {
//    int c;
    for (auto i=start; i!=end; ++i) {
        Op &op=*i;
        switch (op.fn) {
            case ADD: curve.add( *(typename Curve::Point *)(op.r), *(typename Curve::Point *)(op.a), *(typename Curve::Point *)(op.b) ); break;
            case ADD_MIXED: curve.add( *(typename Curve::Point *)(op.r), *(typename Curve::Point *)(op.a), *(typename Curve::PointAffine *)(op.b) ); break;
            case ADD_AFFINE: curve.add( *(typename Curve::Point *)(op.r), *(typename Curve::PointAffine *)(op.a), *(typename Curve::PointAffine *)(op.b) ); break;
        }
//        c++;
    }
//    std::cout <<  "Count: " << c << "\n"; 
}
