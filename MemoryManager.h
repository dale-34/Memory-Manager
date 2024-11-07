#include <iostream>
#include <functional>
#include <string>
#include <cmath>
#include <array>
#include <sstream>
#include <fstream>
#include <vector>
#include <bitset>
#include <unistd.h>
#include <fcntl.h>

using namespace std;


int bestFit(int wordSize, void *list);
int worstFit(int wordSize, void *list);

struct memoryAttributes {
    bool isHole;
    int offset;
    int size;
};

class MemoryManager {
    private:
        unsigned wordSize;
        unsigned totalWords;
        size_t wordsAvailable;
        unsigned totalBytes;
        char *arrPtr;
        char *memoryArray;
        char *bitmap;

        vector<memoryAttributes> memoryVector;
        function<int(int, void *)> defaultAllocator;
    public: 
        MemoryManager(unsigned wordSize, function<int(int, void *)> allocator);
        void initialize(size_t sizeInWords);
        void shutdown();
        void *getList();
        void *allocate(size_t sizeInBytes);
        void free(void* address);
        void setAllocator(function<int(int, void *)> allocator);
        void *getBitmap();
        void *getMemoryStart();
        unsigned getMemoryLimit();
        unsigned getWordSize();
        int dumpMemoryMap(char *filename);
        ~MemoryManager();

};