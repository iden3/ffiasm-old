#ifndef GROWABLE_ARRAY_H
#define GROWABLE_ARRAY_H

template<class T>
class GrowableArray {
    T **pages;
    uint64_t m_size;
    uint64_t m_pageSize;
    uint32_t m_nPages;
    uint32_t m_nReservedPages;

    void checkSize(uint64_t page) {
        if (page<m_nPages) return;
        if (page>=m_nReservedPages) {
            m_nReservedPages = page+1;
            pages = (T **)realloc(pages, sizeof(T *)*m_nReservedPages);
        }
        while (page >= m_nPages) {
            pages[m_nPages++] = new T[m_pageSize]; 
        } 
    }

public:
    GrowableArray(uint64_t pageSize = 1<<20): m_size(0), m_pageSize(pageSize) {
        m_nReservedPages = 256;
        pages = (T **)malloc(sizeof(T *)*m_nReservedPages);
    };
    ~GrowableArray() {
        for (uint64_t i=0; i<m_nPages; i++) delete pages[i];
        free(pages);
    }
    void push_back(T e) {
        uint64_t page = m_size / m_pageSize;
        checkSize(page);
        pages[page][m_size % m_pageSize] = e;
        m_size++;
    }
    void push_back() {
        uint64_t page = m_size / m_pageSize;
        checkSize(page);
        m_size++;
    }
    T& operator[](uint64_t idx) { return pages[idx / m_pageSize][idx % m_pageSize]; }
    uint64_t size() { return m_size; }
};
#endif // GROWABLE_ARRAY_H