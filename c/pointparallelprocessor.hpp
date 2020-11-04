#ifndef POINT_PARALLEL_PROCESSOR_H
#define POINT_PARALLEL_PROCESSOR_H

#include <vector>
#include <thread>
#include <mutex>
#include "growablearray.hpp"

#define NOPS_CHUNK 1<<14
#define MAX_LEVELS 1024
template <typename Curve>
class PointParallelProcessor {

    Curve &curve;
    enum Function { ADD, ADD_MIXED, ADD_AFFINE };
    struct Op {
        Function fn;
        void *r;
        void *a;
        void *b;
        Op(Function _fn, void * _r,void *_a, void *_b) : fn(_fn), r(_r), a(_a), b(_b) {}; 
        Op() {};
    };

public:
    enum Source { ZERO=0, BASE=1, HEAP=2};

    #pragma pack(push, 1)
    struct Point {
        Source source;
        uint16_t level;
        uint32_t idx;

        Point (Source _source, uint16_t _level, uint32_t _idx) : source(_source), level(_level), idx(_idx) {};
    };
    #pragma pack(pop)

private:
    typename Curve::PointAffine *bases;
    GrowableArray<typename Curve::Point> heap;
    GrowableArray<Op> ops[MAX_LEVELS+1];
    u_int32_t nLevels;

    bool terminated;
    uint32_t currentLevel;
    uint64_t nOpsExecuting;
    uint64_t nOpsExecuted;
    uint64_t nOpsCommited;
    std::vector<std::thread> threads;
    std::mutex mutex;
    std::condition_variable cv;

    void addOp(uint32_t level, Function fn, Point r, Point a, Point b);
    Point allocHeapPoint(uint32_t level);
    void *getPointPointer(Point p);

    void childThread();
    void innerProcess(uint32_t level, uint64_t start, uint64_t end);


public:

    PointParallelProcessor(Curve &_curve, uint32_t nThreads, typename Curve::PointAffine *bases);
    ~PointParallelProcessor();

    Point add(Point a, Point b);

    void terminateCalculus();

    void extractResult(typename Curve::Point &r, Point v);
    Point basePoint(uint32_t idx) { return Point(BASE, 0, idx); };
    Point zero() { return Point(ZERO, 0, 0); };
};

#include "pointparallelprocessor.cpp"

#endif // POINT_PARALLEL_PROCESSOR_H