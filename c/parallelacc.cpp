
template <class Processor>
void ParallelAcc<Processor>::add(Processor &prc, Point p) {
    Tree *oldFirstTree = firstTree;
    firstTree = new Tree(1, p, oldFirstTree);

    while ((firstTree->next)&&(firstTree->nLevels == firstTree->next->nLevels)) {
        Tree *oldNext = firstTree->next;
        firstTree->p = prc.add(firstTree->p, firstTree->next->p);
        firstTree->nLevels ++;
        firstTree->next = firstTree->next->next;
        delete oldNext;
    } 
}

template <class Processor>
ParallelAcc<Processor>::~ParallelAcc() {
    Tree *aux = firstTree;
    while (aux) {
        Tree *oldNext = aux->next;
        delete aux;
        aux = oldNext;
    }
}

template <class Processor>
typename ParallelAcc<Processor>::Point ParallelAcc<Processor>::getPoint(Processor &prc) {
    if (!firstTree) return prc.zero();
    
    while (firstTree->next) {
        Tree *oldNext = firstTree->next;
        firstTree->p = prc.add(firstTree->p, firstTree->next->p);
        firstTree->next = firstTree->next->next;
        delete oldNext;
    }

    return firstTree->p;
}
