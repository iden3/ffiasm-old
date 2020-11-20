
template <class Processor>
void ParallelAcc<Processor>::add(Processor &prc, Point p) {
    Tree *oldFirstTree = firstTree;
    firstTree = mm.allocTree();
    firstTree->nLevels = 1;
    firstTree->p = p;
    firstTree->next = oldFirstTree;

    while ((firstTree->next)&&(firstTree->nLevels == firstTree->next->nLevels)) {
        Tree *oldNext = firstTree->next;

        firstTree->p = prc.add(firstTree->p, firstTree->next->p);
        firstTree->nLevels ++;
        firstTree->next = firstTree->next->next;
        mm.freeTree(oldNext);
    } 
}

template <class Processor>
ParallelAcc<Processor>::~ParallelAcc() {
    while (firstTree) {
        Tree *aux = firstTree;
        firstTree = aux->next;
        mm.freeTree(aux);
    }
}

template <class Processor>
typename ParallelAcc<Processor>::Point ParallelAcc<Processor>::getPoint(Processor &prc) {
    if (!firstTree) return prc.zero();
    
    while (firstTree->next) {
        Tree *oldNext = firstTree->next;
        firstTree->p = prc.add(firstTree->p, firstTree->next->p);
        firstTree->next = firstTree->next->next;
        mm.freeTree(oldNext);
    }

    return firstTree->p;
}

template <class Processor>
thread_local typename ParallelAcc<Processor>::MM ParallelAcc<Processor>::mm;

/*
template <class Processor>
inline typename ParallelAcc<Processor>::Tree *ParallelAcc<Processor>::allocTree() {
    return new Tree;
}

template <class Processor>
inline void ParallelAcc<Processor>::freeTree(Tree *t) {
    delete t;
}
*/