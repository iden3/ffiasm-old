#ifndef BINFILE_UTILS_H
#define BINFILE_UTILS_H
#include <string>
#include <map>
#include <vector>
#include <memory>

namespace BinFileUtils
{

    class BinFile
    {

        void *addr;
        u_int64_t size;
        u_int64_t pos;

        class Section
        {
            void *start;
            u_int64_t size;

        public:
            friend BinFile;
            Section(void *_start, u_int64_t _size) : start(_start), size(_size){};
        };

        std::map<int, std::vector<Section>> sections;
        std::string type;
        u_int32_t version;

        Section *readingSection;
        Section *writingSection;

    public:
        BinFile(void *data, uint64_t size, std::string type, uint32_t maxVersion);
        BinFile(std::string fileName, std::string type, uint32_t maxVersion);
        BinFile(std::string fileName, std::string type, uint32_t version, uint32_t nSections);

        ~BinFile();

        void startReadSection(u_int32_t sectionId, u_int32_t sectionPos = 0);
        void endReadSection(bool check = true);

        void *getSectionData(u_int32_t sectionId, u_int32_t sectionPos = 0);
        u_int64_t getSectionSize(u_int32_t sectionId, u_int32_t sectionPos = 0);

        u_int32_t readU32LE();
        u_int64_t readU64LE();

        void *read(uint64_t l);

        std::string readString();

        void startWriteSection(u_int32_t sectionId, u_int32_t sectionPos = 0);
        void endWriteSection();

        void writeU32LE(u_int32_t value);
        void writeU64LE(u_int64_t value);
        void write(void *buffer, u_int64_t len);
        void writeString(const std::string& str);
    };

    std::unique_ptr<BinFile> openExisting(std::string filename, std::string type, uint32_t maxVersion);
}

#endif // BINFILE_UTILS_H
