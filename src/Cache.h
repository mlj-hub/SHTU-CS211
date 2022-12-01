/*
 * Basic cache simulator
 *
 * Created by He, Hao on 2019-4-27
 */

#ifndef CACHE_H
#define CACHE_H

#include <cstdint>
#include <vector>

#include "MemoryManager.h"
#include "State.h"

class MemoryManager;

struct Block {
  bool valid;
  bool modified;
  uint32_t tag;
  uint32_t id;
  uint32_t size;
  uint32_t lastReference;
  std::vector<uint8_t> data;
  State state;
  Block():valid(0),modified(0),tag(0),id(0),size(0),lastReference(0),state(State::Invalid) {}
  Block(const Block &b)
      : valid(b.valid), modified(b.modified), tag(b.tag), id(b.id),
        size(b.size),state(b.state) {
    data = b.data;
  }
};

class Cache {
public:

  struct Policy {
    // In bytes, must be power of 2
    uint32_t cacheSize;
    uint32_t blockSize;
    uint32_t blockNum;
    uint32_t associativity;
    uint32_t hitLatency;  // in cycles
    uint32_t missLatency; // in cycles
  };



  struct Statistics {
    uint32_t numRead;
    uint32_t numWrite;
    uint32_t numHit;
    uint32_t numMiss;
    uint64_t totalCycles;
  };

  Cache(MemoryManager *manager, Policy policy,uint8_t level,Cache *lowerCache = nullptr,pthread_mutex_t* lock=nullptr,
        bool writeBack = true, bool writeAllocate = true);

  bool inCache(uint32_t addr);
  bool inShared(uint32_t addr);
  uint32_t getBlockId(uint32_t addr);
  uint8_t getByte(uint32_t addr, uint32_t *cycles = nullptr);
  void setByte(uint32_t addr, uint8_t val, uint32_t *cycles = nullptr);
  void passSharedBlockToHigherLevel(uint32_t addr, Block * block, uint32_t * cycles);

  void printInfo(bool verbose);
  void printStatistics();

  State getBlockState(uint32_t addr);
  void invalidCacheBlock(uint32_t addr);
  void setState(uint32_t addr,State state);
  void findSetStateAll(uint32_t addr, State state,std::vector<uint8_t> &data);
  void setStateAll(uint32_t addr,State state);
  uint32_t getReplacementBlockId(uint32_t begin, uint32_t end);
  uint32_t getReplacementBlockIdLLC(uint32_t begin, uint32_t end);

  Statistics statistics;
  uint8_t level;

  uint32_t referenceCounter;
  bool writeBack;     // default true
  bool writeAllocate; // default true
  MemoryManager *memory;
  Cache *lowerCache;
  Policy policy;
  std::vector<Block> blocks;
  pthread_mutex_t *lock;

private:

  void initCache();
  void loadBlockFromLowerLevel(uint32_t addr, uint32_t *cycles = nullptr);
  void loadSharedBlockFromLowerLevel(uint32_t addr, uint32_t *cycles = nullptr);

  void writeBlockToLowerLevel(Block &b);
  void handleWriteBack(Block b);
  void handleLLCWriteBack(Block b);
  void handlePrivateReadHit(uint32_t addr,Block*dst,Block*src);

  // Utility Functions
  bool isPolicyValid();
  bool isPowerOfTwo(uint32_t n);
  uint32_t log2i(uint32_t val);
  uint32_t getTag(uint32_t addr);
  uint32_t getId(uint32_t addr);
  uint32_t getOffset(uint32_t addr);
  uint32_t getAddr(Block &b);
};

#endif