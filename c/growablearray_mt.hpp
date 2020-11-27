#ifndef GROWABLE_ARRAY_MT_H
#define GROWABLE_ARRAY_MT_H

#include <map>
#include <shared_mutex>

template<class T>
class GrowableArrayMT {
    struct Page {
        uint32_t n;
        T *elements;
        Page *next;
    };

    struct PageList {
        Page *p;
    };

public:
    struct Iterator {
        GrowableArrayMT *ga;
        uint32_t idThread;
        Page *page;
        uint32_t idx;

        void adjustIterator() {
            if ((page)&&(page->n == idx)) {
                page = page->next;
                idx = 0;
            }
            while (!page) {
                idThread ++;
                idx = 0;
                if (idThread == ga->nThreads) {
                    return;
                }
                page = ga->th2page[idThread].p;
            }
        }

    public:
        Iterator operator +(uint64_t inc) {
            Iterator it = *this;
            while ((it.idThread < ga->nThreads)&&(inc>0)) {
                uint32_t dec = it.page->n - it.idx > inc ? inc : it.page->n - it.idx;
                it.idx += dec;
                inc -= dec;
                it.adjustIterator();
            }
            return it;
        }
        Iterator &operator ++() { 
            if ((page)&&(idx <page->n-1)) {
                idx++;
                return *this;
            }
            *this = (*this) + 1;
            return *this;
        }
        friend bool operator==(const Iterator&a, const Iterator&b) {
            return ((a.ga == b.ga) &&
                    (a.page == b.page) &&
                    (a.idx == b.idx));
        }
        friend bool operator!=(const Iterator&a, const Iterator&b) {
            return !(a==b);
        }
        T &operator*() const {
            return page->elements[idx];
        };
        T *operator->() const {
            return &(page->elements[idx]);
        };
    };

private:

    PageList *th2page;
    uint32_t m_pageSize;
    uint32_t nThreads;

public:
    GrowableArrayMT(uint32_t _nThreads, uint64_t _pageSize = 1<<18) {
        nThreads = _nThreads==0 ? std::thread::hardware_concurrency() : _nThreads;
        m_pageSize = _pageSize;
        th2page = new PageList[nThreads]();
    };

    ~GrowableArrayMT() {
        for (uint32_t i=0; i<nThreads; i++) {
            while (th2page[i].p) {
                Page *p = th2page[i].p;
                th2page[i].p = p->next;
                delete[] p->elements;
                delete p;
            }
        }
        delete[] th2page;
    }

    T &addElement(uint32_t idThread) {
        if (idThread >= nThreads) {
            throw std::range_error("Thread out of range");
        }
        if ((!th2page[idThread].p) || (th2page[idThread].p->n ==  m_pageSize)) {
            Page *p= new Page;
            p->n =0;
            p->elements = new T[m_pageSize+64];
            p->next = th2page[idThread].p;
            th2page[idThread].p = p;
        }
        return th2page[idThread].p->elements[th2page[idThread].p->n++];
    }
    Iterator begin() {
        Iterator it;
        it.ga = this;
        it.idThread = 0;
        it.page = th2page[0].p;
        it.idx = 0;
        it.adjustIterator();
        return it;
    }
    Iterator end() {
        Iterator it;
        it.ga = this;
        it.idThread = nThreads;
        it.page = 0;
        it.idx = 0;
        return it;
    }
};
#endif // GROWABLE_ARRAY_MT_H