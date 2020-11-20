#ifndef PARALLEL_ACCUMULATOR
#define PARALLEL_ACCUMULATOR

#include "pointparallelprocessor.hpp"

template <class Processor>
class ParallelAcc {
    typedef typename Processor::Point Point;

    struct Tree {
        Point p;
        Tree *next;
        int nLevels;

        Tree() {};
    };

    struct TreePage {
        Tree *trees;
        TreePage *next;
    };

    class MM {
        TreePage *treePages;
        Tree *freeTrees;

        int allocationChunk;

        void allocPage() {
            TreePage *p = new TreePage;
            p->trees = new Tree[allocationChunk];
            p->next = treePages;
            treePages = p;
            for (int i=0; i<allocationChunk-1; i++) {
                p->trees[i].next = &(p->trees[i+1]);
            }
            p->trees[allocationChunk-1].next = freeTrees;
            freeTrees = &(p->trees[0]);
        }
    public:
        MM(int _allocationChunk = 1024) : allocationChunk(_allocationChunk), treePages(NULL) {}
        ~MM() {
            while (treePages!=NULL) {
                TreePage *oldTreePage = treePages;
                treePages = treePages->next;
                delete oldTreePage->trees;
                delete oldTreePage;
            }
        }
        Tree *allocTree() {
            if (!freeTrees) allocPage();
            Tree *r = freeTrees;
            freeTrees = freeTrees -> next;
            return r;
        }
        void freeTree(Tree *t) {
            t->next = freeTrees;
            freeTrees = t;
        }        
    };


    Tree *firstTree;

    static thread_local MM mm;
public:

    ParallelAcc() : firstTree(NULL) {};
    ~ParallelAcc();

    void add(Processor &prc, Point p);

    Point getPoint(Processor &prc);
};

#include "parallelacc.cpp"

#endif