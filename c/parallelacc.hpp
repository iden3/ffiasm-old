#ifndef PARALLEL_ACCUMULATOR
#define PARALLEL_ACCUMULATOR

#include "pointparallelprocessor.hpp"

template <class Processor>
class ParallelAcc {
    typedef typename Processor::Point Point;

    struct Tree {
        int nLevels;
        Point p;
        Tree *next;

        Tree(int _nLevels, Point _p, Tree *_next) : nLevels(_nLevels), p(_p), next(_next) {};
    };


    Tree *firstTree;
public:

    ParallelAcc() : firstTree(NULL) {};
    ~ParallelAcc();

    void add(Processor &prc, Point p);

    Point getPoint(Processor &prc);
};

#include "parallelacc.cpp"

#endif