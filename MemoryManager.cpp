#include "MemoryManager.h"

MemoryManager::MemoryManager(unsigned wordSize, function<int(int, void *)> allocator) {
    this->wordSize = wordSize;
    memoryArray = nullptr;
    arrPtr = nullptr;
    bitmap = nullptr;
    defaultAllocator = allocator;
    totalWords = 0;
    wordsAvailable = 0;
    totalBytes = 0;
}


void MemoryManager::initialize(size_t sizeInWords) {
    if (memoryArray != nullptr) {
        shutdown();
    }
    sizeInWords = (sizeInWords <= 65536) ? sizeInWords : 65536; 
    wordsAvailable = sizeInWords;
    totalWords = sizeInWords;
    totalBytes = sizeInWords*wordSize;
    memoryArray = new char[totalBytes];

    memoryAttributes initial = {true, 0, static_cast<int>(sizeInWords)};
    memoryVector.push_back(initial);
}

void MemoryManager::shutdown() {
    if (memoryArray != nullptr) {
        delete[] memoryArray;
        memoryArray = nullptr;
    }
    memoryVector.clear();
}

void *MemoryManager::getList() {
    short int holeCount = 0;

    for(int i = 0; i < memoryVector.size(); i++) {
        if (memoryVector[i].isHole) {
            holeCount++;
        }
    }

    int arrSize = (holeCount * 2) + 1;
    short int *holeArray = new short int[arrSize];
    holeArray[0] = holeCount;
    int index = 1;

    for (int i = 0; i < memoryVector.size(); i++) {
        if (memoryVector[i].isHole && index + 1 <= arrSize && index < arrSize) {
            holeArray[index] = static_cast<short int>(memoryVector[i].offset);
            holeArray[index+1] = static_cast<short int>(memoryVector[i].size);
            index += 2;
        }
    }

    return holeArray;
}

void *MemoryManager::allocate(size_t sizeInBytes) {
    int wordAllocate = (sizeInBytes > wordSize) ? static_cast<int>(ceil(sizeInBytes/wordSize)) : 1;

    if (wordsAvailable == 0) {
        return nullptr;
    }

    short int *holeList = static_cast<short int*>(getList());
    int pos = defaultAllocator(wordAllocate, holeList);

    for (int i = 0; i < memoryVector.size(); i++) {
        if (memoryVector[i].offset == pos) {
            // Create New Holes
            if (wordsAvailable != 0 && memoryVector[i].offset >= memoryVector.back().offset) {
                wordsAvailable -= wordAllocate;
                int addOffset = wordAllocate + pos;
                int addSize = wordsAvailable;
                memoryAttributes tracking = {true, addOffset, addSize};
                memoryVector.push_back(tracking);
            }
            //Edit Existing Holes
            else if (memoryVector[i].size - wordAllocate != 0 && (i+1 < memoryVector.size())) {
                auto it = memoryVector.begin() + i + 1;
                int addOffset = memoryVector[i].offset + wordAllocate;
                int addSize = memoryVector[i+1].offset - addOffset;
                memoryAttributes addHole = {true,addOffset, addSize};
                memoryVector.insert(it, addHole);
            }
            arrPtr = &memoryArray[pos*wordSize];
            memoryVector[i].isHole = false;
            memoryVector[i].size = wordAllocate;
        }
    }

    delete[] holeList;
    return arrPtr;
}

void MemoryManager::free(void *address) {
    for (int i = 0; i < memoryVector.size(); i++) {
        if (&memoryArray[memoryVector[i].offset*wordSize] == address) {
            memoryVector[i].isHole = true;
            // Check forward existing holes
            if (i+1 < memoryVector.size()) {
                if (memoryVector[i].offset + memoryVector[i].size == memoryVector[i+1].offset && memoryVector[i+1].isHole) {
                    int addOffset = memoryVector[i].offset;
                    int addSize = memoryVector[i].size + memoryVector[i+1].size;
                    memoryAttributes add = {true, addOffset, addSize};
                    auto itOne = memoryVector.begin() + i;
                    auto itTwo = memoryVector.begin() + i;
                    memoryVector.erase(itOne);
                    memoryVector.erase(itTwo);
                    memoryVector.insert(itOne, add);
                }
            }
            // Check backwards existing holes
            if (i - 1 > 0) {
                if (memoryVector[i].offset == memoryVector[i-1].offset + memoryVector[i-1].size && memoryVector[i-1].isHole) {
                    int addOffset = memoryVector[i-1].offset;
                    int addSize = memoryVector[i].size + memoryVector[i-1].size;
                    memoryAttributes add = {true, addOffset, addSize};
                    auto itOne = memoryVector.begin() + i;
                    auto itTwo = memoryVector.begin() + i - 1;
                    memoryVector.erase(itOne);
                    memoryVector.erase(itTwo);
                    memoryVector.insert(itTwo, add);
                }
            }
            break;
        }
    }
}

void MemoryManager::setAllocator(std::function<int (int, void *)> allocator) {
    defaultAllocator = allocator;
}

void *MemoryManager::getBitmap() {
    unsigned size;

    // Extend bitmap if necessary
    if (totalWords % 8 == 0) {
        size = totalWords;
    } else {
        size = totalWords + (8 - (totalWords % 8));
    }

    int bitstream[size];
    int index = 0;
    int extend = 0;
    int count = 1;

    // Calculates holes and blocks to 1's and 0's 
    for (int i = 0; i < memoryVector.size(); i++) {
        if (index == size) {
            break;
        }
        while (count <= memoryVector[i].size) {
            if (memoryVector[i].isHole) {
                bitstream[index] = 0;
            } else {
                bitstream[index] = 1;
            }
            index++;
            count++;
        }
        count = 1;
    }

    // Fills the rest of bitstream
    while (index < size) {
        bitstream[index] = extend;
        index++;
    }

    int start = 0;
    unsigned stop = 8;
    int bitIndex = 0;
    bitset<8> bytes;
    string wholeBitmap;

    bitset<16> byteSize(size/8);
    int switchIndex = 8;

    // Perform Little Endian for size
    for (int i = 0; i < 8; i++) {
        bool temp = byteSize[i];
        byteSize[i] = byteSize[switchIndex];
        byteSize[switchIndex] = temp;
        switchIndex++;
    }
    wholeBitmap += byteSize.to_string();

    // Mirrors holes and blocks 
    while (stop <= size) {
        for (int i = start; i < stop; i++) {
            bytes[bitIndex] = bitstream[i];
            bitIndex++;
        }
        wholeBitmap += bytes.to_string();
        bitIndex = 0;
        start += 8;
        stop += 8;
    }

    // Convert binary to decimal, store in bitmap array
    unsigned bitmapSize = (size + 16) / 8;
    bitmap = new char[bitmapSize];
    int bitMapIndex = 0;
    for (int i = 0; i < wholeBitmap.length(); i += 8) {
        if (bitMapIndex <= bitmapSize) {
            string eightBits = wholeBitmap.substr(i, 8);
            unsigned long decimal = bitset<8>(eightBits).to_ulong();
            bitmap[bitMapIndex] = static_cast<char>(decimal);
            bitMapIndex++;
        }
    }

    return bitmap;
}

void *MemoryManager::getMemoryStart() {
    return memoryArray;
}

unsigned int MemoryManager::getMemoryLimit() {
    return totalBytes;
}

unsigned int MemoryManager::getWordSize() {
    return wordSize;
}

int MemoryManager::dumpMemoryMap(char *filename) {
    short int *listParam = static_cast<short int*>(getList());
    int size = listParam[0]*2;
    string toWrite;

    int file = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0777);
    if (file == -1) {
        return 1;
    }

    for (int i = 1; i < size; i += 2) {
        if ((i+1) <= size) {
            toWrite += "[" + to_string(listParam[i]) + ", " + to_string(listParam[i + 1]) + "]";
        }
        if (i < size - 1) {
            toWrite += " - ";
        }
    }

    ssize_t bytes_written = write(file, toWrite.c_str(), toWrite.size());
    if (bytes_written == -1) {
        close(file);
        return 1;
    }
    close(file);
    delete[] listParam;
    return 0;
}

MemoryManager::~MemoryManager() {
    shutdown();
}

// Allocators
int bestFit(int wordSize, void *list) {
    short int *listParam = static_cast<short int*>(list);
    int arrSize = listParam[0] * 2;
    int offset = listParam[1];
    int smallestHole = listParam[2];

    for (int i = 2; i < arrSize; i += 2) {
        if (abs(listParam[i] - wordSize) < smallestHole) {
            smallestHole = abs(listParam[i] - wordSize);
            offset = listParam[i-1];
        }
    }
    return offset;
}

int worstFit(int wordSize, void *list) {
    short int *listParam = static_cast<short int*>(list);
    int arrSize = listParam[0] * 2;
    int largestHole = listParam[2];
    int offset = listParam[1];

    for (int i = 2; i < arrSize; i += 2) {
        if (abs(listParam[i] - wordSize) > largestHole) {
            largestHole = abs(listParam[i] - wordSize);
            offset = listParam[i-1];
        }
    }
    return offset;
}
