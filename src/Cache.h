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

class MemoryManager;

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

  struct Block {
    bool valid;
    bool dead;
    bool modified;
    uint32_t tag;
    uint32_t id;
    uint32_t size;
    uint32_t lastReference;
    std::vector<uint8_t> data;
    Block() {dead = 0;}
    Block(const Block &b)
        : valid(b.valid), modified(b.modified), tag(b.tag), id(b.id),
          size(b.size) {
      data = b.data;
    }
  };

  struct Statistics {
    uint32_t numRead;
    uint32_t numWrite;
    uint32_t numHit;
    uint32_t numMiss;
    uint64_t totalCycles;
  };

  struct Sampler{
    struct SamplerEntry{
      uint32_t trace;
      uint32_t tag;
      uint8_t valid;
      uint32_t lastReference;
      SamplerEntry(){
        trace = 0;
        tag = 0;
        valid = 0;
        lastReference = 0;
      };
    };
    struct Predictor{
      std::vector<uint8_t> counter1 = std::vector<uint8_t> (4096);
      std::vector<uint8_t> counter2 = std::vector<uint8_t> (4096);
      std::vector<uint8_t> counter3 = std::vector<uint8_t> (4096);
      void updateCounter(uint32_t idx1,uint32_t idx2,uint32_t idx3);
    };
    uint32_t hash1(uint32_t trace,uint32_t tag);
    uint32_t hash2(uint32_t trace,uint32_t tag);
    uint32_t hash3(uint32_t trace,uint32_t tag);
    void updateTraceWhenAccess(uint32_t setId,uint32_t tag);
    void updatePredictorWhenReplace(uint32_t setId,uint32_t tag);
    uint8_t prediction(uint32_t pc,uint32_t tag);
    uint32_t setNum=32;
    uint32_t associativity = 12;
    uint32_t referenceCycle=0;
    std::vector<SamplerEntry> entries = std::vector<SamplerEntry>(32*12);
    Predictor predictor;
  };



  Cache(MemoryManager *manager, Policy policy, Cache *lowerCache = nullptr,uint8_t level=1,
        bool writeBack = true, bool writeAllocate = true);

  bool inCache(uint32_t addr);
  uint32_t getBlockId(uint32_t addr);
  uint8_t getByte(uint32_t addr, uint32_t *cycles = nullptr);
  void setByte(uint32_t addr, uint8_t val, uint32_t *cycles = nullptr);

  void printInfo(bool verbose);
  void printStatistics();

  Statistics statistics;

private:
  uint32_t referenceCounter;
  bool writeBack;     // default true
  bool writeAllocate; // default true
  MemoryManager *memory;
  Cache *lowerCache;
  Policy policy;
  std::vector<Block> blocks;
  uint8_t level;
  Sampler sampler;

  void initCache();
  void loadBlockFromLowerLevel(uint32_t addr, uint32_t *cycles = nullptr);
  uint32_t getReplacementBlockId(uint32_t begin, uint32_t end);
  void writeBlockToLowerLevel(Block &b);

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