#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <system_error>
#include <string>
#include <memory.h>
#include <stdexcept>

#include "binfile_utils.hpp"
#include "thread_utils.hpp"
#include <omp.h>

namespace BinFileUtils
{
    BinFile::BinFile(void *data, uint64_t size, std::string _type, uint32_t maxVersion)
    {
        addr = malloc(size);
        int nThreads = omp_get_max_threads() / 2;
        ThreadUtils::parcpy(addr, data, size, nThreads);

        type.assign((const char *)addr, 4);
        pos = 4;

        if (type != _type)
        {
            throw new std::invalid_argument("Invalid file type. It should be " + _type + " and it is " + type);
        }

        version = readU32LE();
        if (version > maxVersion)
        {
            throw new std::invalid_argument("Invalid version. It should be <=" + std::to_string(maxVersion) + " and it us " + std::to_string(version));
        }

        u_int32_t nSections = readU32LE();

        for (u_int32_t i = 0; i < nSections; i++)
        {
            u_int32_t sType = readU32LE();
            u_int64_t sSize = readU64LE();

            if (sections.find(sType) == sections.end())
            {
                sections.insert(std::make_pair(sType, std::vector<Section>()));
            }

            sections[sType].push_back(Section((void *)((u_int64_t)addr + pos), sSize));

            pos += sSize;
        }

        pos = 0;
        readingSection = NULL;
    }

    BinFile::BinFile(std::string fileName, std::string _type, uint32_t maxVersion)
    {

        int fd;
        struct stat sb;

        fd = open(fileName.c_str(), O_RDONLY);
        if (fd == -1)
            throw std::system_error(errno, std::generic_category(), "open");

        if (fstat(fd, &sb) == -1) /* To obtain file size */
            throw std::system_error(errno, std::generic_category(), "fstat");

        size = sb.st_size;
        void *addrmm = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
        addr = malloc(sb.st_size);

        int nThreads = omp_get_max_threads() / 2;
        ThreadUtils::parcpy(addr, addrmm, sb.st_size, nThreads);

        munmap(addrmm, sb.st_size);
        close(fd);

        type.assign((const char *)addr, 4);
        pos = 4;

        if (type != _type)
        {
            throw new std::invalid_argument("Invalid file type. It should be " + _type + " and it us " + type);
        }

        version = readU32LE();
        if (version > maxVersion)
        {
            throw new std::invalid_argument("Invalid version. It should be <=" + std::to_string(maxVersion) + " and it us " + std::to_string(version));
        }

        u_int32_t nSections = readU32LE();

        for (u_int32_t i = 0; i < nSections; i++)
        {
            u_int32_t sType = readU32LE();
            u_int64_t sSize = readU64LE();

            if (sections.find(sType) == sections.end())
            {
                sections.insert(std::make_pair(sType, std::vector<Section>()));
            }

            sections[sType].push_back(Section((void *)((u_int64_t)addr + pos), sSize));

            pos += sSize;
        }

        pos = 0;
        readingSection = NULL;
    }

    BinFile::BinFile(std::string fileName, std::string type, uint32_t version, uint32_t nSections)
    {
        pos = 0;

        std::vector<char> bytes(4);
        for (int i = 0; i < 4; i++) bytes[i] = type.at(i);
        
        write(bytes.data(), 4);

        writeU32LE(version);

        writeU32LE(nSections);

        writingSection = NULL;
    }

    BinFile::~BinFile()
    {
        free(addr);
    }

    void BinFile::startReadSection(u_int32_t sectionId, u_int32_t sectionPos)
    {

        if (sections.find(sectionId) == sections.end())
        {
            throw new std::range_error("Section does not exist: " + std::to_string(sectionId));
        }

        if (sectionPos >= sections[sectionId].size())
        {
            throw new std::range_error("Section pos too big. There are " + std::to_string(sections[sectionId].size()) + " and it's trying to access section: " + std::to_string(sectionPos));
        }

        if (readingSection != NULL)
        {
            throw new std::range_error("Already reading a section");
        }

        pos = (u_int64_t)(sections[sectionId][sectionPos].start) - (u_int64_t)addr;

        readingSection = &sections[sectionId][sectionPos];
    }

    void BinFile::endReadSection(bool check)
    {
        if (check)
        {
            if ((u_int64_t)addr + pos - (u_int64_t)(readingSection->start) != readingSection->size)
            {
                throw new std::range_error("Invalid section size");
            }
        }
        readingSection = NULL;
    }

    void *BinFile::getSectionData(u_int32_t sectionId, u_int32_t sectionPos)
    {

        if (sections.find(sectionId) == sections.end())
        {
            throw new std::range_error("Section does not exist: " + std::to_string(sectionId));
        }

        if (sectionPos >= sections[sectionId].size())
        {
            throw new std::range_error("Section pos too big. There are " + std::to_string(sections[sectionId].size()) + " and it's trying to access section: " + std::to_string(sectionPos));
        }

        return sections[sectionId][sectionPos].start;
    }

    u_int64_t BinFile::getSectionSize(u_int32_t sectionId, u_int32_t sectionPos)
    {

        if (sections.find(sectionId) == sections.end())
        {
            throw new std::range_error("Section does not exist: " + std::to_string(sectionId));
        }

        if (sectionPos >= sections[sectionId].size())
        {
            throw new std::range_error("Section pos too big. There are " + std::to_string(sections[sectionId].size()) + " and it's trying to access section: " + std::to_string(sectionPos));
        }

        return sections[sectionId][sectionPos].size;
    }

    u_int32_t BinFile::readU32LE()
    {
        u_int32_t res = *((u_int32_t *)((u_int64_t)addr + pos));
        pos += 4;
        return res;
    }

    u_int64_t BinFile::readU64LE()
    {
        u_int64_t res = *((u_int64_t *)((u_int64_t)addr + pos));
        pos += 8;
        return res;
    }

    void *BinFile::read(u_int64_t len)
    {
        void *res = (void *)((u_int64_t)addr + pos);
        pos += len;
        return res;
    }

    std::string BinFile::readString()
    {
        uint8_t *startOfString = (uint8_t *)((u_int64_t)addr + pos);
        uint8_t *endOfString = startOfString;
        uint8_t *endOfSection = (uint8_t *)((uint64_t)readingSection->start + readingSection->size);

        uint8_t *i;
        for (i = endOfString; i != endOfSection; i++)
        {
            if (*i == 0)
            {
                endOfString = i;
                break;
            }
        }

        if (i == endOfSection)
        {
            endOfString = i - 1;
        }

        uint32_t len = endOfString - startOfString;
        std::string str = std::string((const char *)startOfString, len);
        pos += len + 1;

        return str;
    }

    void BinFile::startWriteSection(u_int32_t sectionId, u_int32_t sectionPos) 
    {
        if (writingSection != NULL)
        {
            throw new std::range_error("Already writing a section");
        }

        sections[sectionId][sectionPos] = Section((void *)((u_int64_t)addr + pos), 0);

        writingSection = &sections[sectionId][sectionPos];

    }

    void BinFile::endWriteSection() 
    {
        if (writingSection == NULL)
        {
            throw new std::range_error("Not writing a section");
        }
                
        writingSection->size = (u_int64_t)addr + pos - (u_int64_t)(writingSection->start);

        writingSection = NULL;
    }

    void BinFile::writeU32LE(u_int32_t value) 
    {
        *(u_int32_t*)((u_int64_t)addr + pos) = value;
        pos += 4;
    }

    void BinFile::writeU64LE(u_int64_t value) 
    {
        *(u_int64_t*)((u_int64_t)addr + pos) = value;
        pos += 8;
    }

    void BinFile::write(void *buffer, u_int64_t len) 
    {
        int nThreads = omp_get_max_threads() / 2;
        ThreadUtils::parcpy((void *)((u_int64_t)addr + pos), buffer, len, nThreads);
        pos += len;
    }

    void BinFile::writeString(const std::string& str) 
    {
        u_int8_t* buff = new u_int8_t[str.length() + 1];
        for (size_t i = 0; i < str.length(); i++) {
            buff[i] = str[i];
        }

        buff[str.length()] = 0;

        write(buff, str.length() + 1);

        delete[] buff;
    }

    std::unique_ptr<BinFile> openExisting(std::string filename, std::string type, uint32_t maxVersion)
    {
        return std::unique_ptr<BinFile>(new BinFile(filename, type, maxVersion));
    }

} // Namespace