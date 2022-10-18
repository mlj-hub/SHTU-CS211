/*
 * Implementation of a simple cache simulator
 *
 * Created By He, Hao in 2019-04-27
 */

#include <cstdio>
#include <cstdlib>

#include "Cache.h"
extern uint32_t memPC;

Cache::Cache(MemoryManager *manager, Policy policy, Cache *lowerCache,uint8_t level,
             bool writeBack, bool writeAllocate) {
  this->referenceCounter = 0;
  this->memory = manager;
  this->policy = policy;
  this->lowerCache = lowerCache;
  if (!this->isPolicyValid()) {
    fprintf(stderr, "Policy invalid!\n");
    exit(-1);
  }
  this->initCache();
  this->statistics.numRead = 0;
  this->statistics.numWrite = 0;
  this->statistics.numHit = 0;
  this->statistics.numMiss = 0;
  this->statistics.totalCycles = 0;
  this->writeBack = writeBack;
  this->writeAllocate = writeAllocate;
  this->level = level;
}

bool Cache::inCache(uint32_t addr) {
  return getBlockId(addr) != -1 ? true : false;
}

uint32_t Cache::getBlockId(uint32_t addr) {
  uint32_t tag = this->getTag(addr);
  uint32_t id = this->getId(addr);
  // printf("0x%x 0x%x 0x%x\n", addr, tag, id);
  // iterate over the given set
  for (uint32_t i = id * policy.associativity;
       i < (id + 1) * policy.associativity; ++i) {
    if (this->blocks[i].id != id) {
      fprintf(stderr, "Inconsistent ID in block %d\n", i);
      exit(-1);
    }
    if (this->blocks[i].valid && this->blocks[i].tag == tag) {
      return i;
    }
  }
  return -1;
}

uint8_t Cache::getByte(uint32_t addr, uint32_t *cycles) {
  this->referenceCounter++;
  this->statistics.numRead++;

  // If in cache, return directly
  int blockId;
  if ((blockId = this->getBlockId(addr)) != -1) {
    if(this->level == 3&& blockId % 512==0){
      this->sampler.updateTraceWhenAccess(blockId/512,this->getTag(addr)&(0x7fff));
    }
    if(this->level == 3){
      uint8_t dead = this->sampler.prediction(memPC&0x7fff,this->blocks[blockId].tag&0x7fff);
      this->blocks[blockId].dead = dead;
    }
    uint32_t offset = this->getOffset(addr);
    this->statistics.numHit++;
    this->statistics.totalCycles += this->policy.hitLatency;
    this->blocks[blockId].lastReference = this->referenceCounter;
    if (cycles) *cycles = this->policy.hitLatency;
    return this->blocks[blockId].data[offset];
  }

  // Else, find the data in memory or other level of cache
  this->statistics.numMiss++;
  this->statistics.totalCycles += this->policy.missLatency;
  this->loadBlockFromLowerLevel(addr, cycles);

  // The block is in top level cache now, return directly
  if ((blockId = this->getBlockId(addr)) != -1) {
    if(this->level == 3&& blockId % 512==0){
      this->sampler.updateTraceWhenAccess(blockId/512,this->getTag(addr)&(0x7fff));
    }
    if(this->level == 3){
      uint8_t dead = this->sampler.prediction(memPC&0x7fff,this->blocks[blockId].tag&0x7fff);
      this->blocks[blockId].dead = dead;
    }
    uint32_t offset = this->getOffset(addr);
    this->blocks[blockId].lastReference = this->referenceCounter;
    return this->blocks[blockId].data[offset];
  } else {
    fprintf(stderr, "Error: data not in top level cache!\n");
    exit(-1);
  }
}

void Cache::setByte(uint32_t addr, uint8_t val, uint32_t *cycles) {
  this->referenceCounter++;
  this->statistics.numWrite++;

  // If in cache, write to it directly
  int blockId;
  if ((blockId = this->getBlockId(addr)) != -1) {
    if(this->level == 3&& blockId % 512==0){
      this->sampler.updateTraceWhenAccess(blockId/512,this->getTag(addr)&(0x7fff));
    }
    if(this->level == 3){
      uint8_t dead = this->sampler.prediction(memPC&0x7fff,this->blocks[blockId].tag&0x7fff);
      this->blocks[blockId].dead = dead;
    }
    uint32_t offset = this->getOffset(addr);
    this->statistics.numHit++;
    this->statistics.totalCycles += this->policy.hitLatency;
    this->blocks[blockId].modified = true;
    this->blocks[blockId].lastReference = this->referenceCounter;
    this->blocks[blockId].data[offset] = val;
    if (!this->writeBack) {
      this->writeBlockToLowerLevel(this->blocks[blockId]);
      this->statistics.totalCycles += this->policy.missLatency;
    }
    if (cycles) *cycles = this->policy.hitLatency;
    return;
  }

  // Else, load the data from cache
  // TODO: implement bypassing
  this->statistics.numMiss++;
  this->statistics.totalCycles += this->policy.missLatency;

  if (this->writeAllocate) {
    this->loadBlockFromLowerLevel(addr, cycles);

    if ((blockId = this->getBlockId(addr)) != -1) {
      if(this->level == 3&& blockId % 512==0){
        this->sampler.updateTraceWhenAccess(blockId/512,this->getTag(addr)&(0x7fff));
      }
      if(this->level == 3){
        uint8_t dead = this->sampler.prediction(memPC&0x7fff,this->blocks[blockId].tag&0x7fff);
        this->blocks[blockId].dead = dead;
      }
      uint32_t offset = this->getOffset(addr);
      this->blocks[blockId].modified = true;
      this->blocks[blockId].lastReference = this->referenceCounter;
      this->blocks[blockId].data[offset] = val;
      return;
    } else {
      fprintf(stderr, "Error: data not in top level cache!\n");
      exit(-1);
    }
  } else {
    if (this->lowerCache == nullptr) {
      this->memory->setByteNoCache(addr, val);
    } else {
      this->lowerCache->setByte(addr, val);
    }
  }
}

void Cache::printInfo(bool verbose) {
  printf("---------- Cache Info -----------\n");
  printf("Cache Size: %d bytes\n", this->policy.cacheSize);
  printf("Block Size: %d bytes\n", this->policy.blockSize);
  printf("Block Num: %d\n", this->policy.blockNum);
  printf("Associativiy: %d\n", this->policy.associativity);
  printf("Hit Latency: %d\n", this->policy.hitLatency);
  printf("Miss Latency: %d\n", this->policy.missLatency);

  if (verbose) {
    for (int j = 0; j < this->blocks.size(); ++j) {
      const Block &b = this->blocks[j];
      printf("Block %d: tag 0x%x id %d %s %s (last ref %d)\n", j, b.tag, b.id,
             b.valid ? "valid" : "invalid",
             b.modified ? "modified" : "unmodified", b.lastReference);
      // printf("Data: ");
      // for (uint8_t d : b.data)
      // printf("%d ", d);
      // printf("\n");
    }
  }
}

void Cache::printStatistics() {
  printf("-------- STATISTICS ----------\n");
  printf("Num Read: %d\n", this->statistics.numRead);
  printf("Num Write: %d\n", this->statistics.numWrite);
  printf("Num Hit: %d\n", this->statistics.numHit);
  printf("Num Miss: %d\n", this->statistics.numMiss);
  printf("Total Cycles: %llu\n", this->statistics.totalCycles);
  if (this->lowerCache != nullptr) {
    printf("---------- LOWER CACHE ----------\n");
    this->lowerCache->printStatistics();
  }
}

bool Cache::isPolicyValid() {
  if (!this->isPowerOfTwo(policy.cacheSize)) {
    fprintf(stderr, "Invalid Cache Size %d\n", policy.cacheSize);
    return false;
  }
  if (!this->isPowerOfTwo(policy.blockSize)) {
    fprintf(stderr, "Invalid Block Size %d\n", policy.blockSize);
    return false;
  }
  if (policy.cacheSize % policy.blockSize != 0) {
    fprintf(stderr, "cacheSize %% blockSize != 0\n");
    return false;
  }
  if (policy.blockNum * policy.blockSize != policy.cacheSize) {
    fprintf(stderr, "blockNum * blockSize != cacheSize\n");
    return false;
  }
  if (policy.blockNum % policy.associativity != 0) {
    fprintf(stderr, "blockNum %% associativity != 0\n");
    return false;
  }
  return true;
}

void Cache::initCache() {
  this->blocks = std::vector<Block>(policy.blockNum);
  for (uint32_t i = 0; i < this->blocks.size(); ++i) {
    Block &b = this->blocks[i];
    b.valid = false;
    b.modified = false;
    b.size = policy.blockSize;
    b.tag = 0;
    b.id = i / policy.associativity;
    b.lastReference = 0;
    b.data = std::vector<uint8_t>(b.size);
  }
}

void Cache::loadBlockFromLowerLevel(uint32_t addr, uint32_t *cycles) {
  uint32_t blockSize = this->policy.blockSize;

  // Initialize new block from memory
  Block b;
  b.valid = true;
  b.modified = false;
  b.tag = this->getTag(addr);
  b.id = this->getId(addr);
  b.size = blockSize;
  b.data = std::vector<uint8_t>(b.size);
  uint32_t bits = this->log2i(blockSize);
  uint32_t mask = ~((1 << bits) - 1);
  uint32_t blockAddrBegin = addr & mask;
  for (uint32_t i = blockAddrBegin; i < blockAddrBegin + blockSize; ++i) {
    if (this->lowerCache == nullptr) {
      b.data[i - blockAddrBegin] = this->memory->getByteNoCache(i);
      if (cycles) *cycles = 100;
    } else 
      b.data[i - blockAddrBegin] = this->lowerCache->getByte(i, cycles);
  }

  // Find replace block
  uint32_t id = this->getId(addr);
  uint32_t blockIdBegin = id * this->policy.associativity;
  uint32_t blockIdEnd = (id + 1) * this->policy.associativity;
  uint32_t replaceId = this->getReplacementBlockId(blockIdBegin, blockIdEnd);
  Block replaceBlock = this->blocks[replaceId];
  if(replaceBlock.valid && this->level == 3 && replaceBlock.id % 512==0){
    this->sampler.updatePredictorWhenReplace(replaceBlock.id/512,replaceBlock.tag&0x7fff);
  }
  if (this->writeBack && replaceBlock.valid &&
      replaceBlock.modified) { // write back to memory
    this->writeBlockToLowerLevel(replaceBlock);
    this->statistics.totalCycles += this->policy.missLatency;
  }

  this->blocks[replaceId] = b;
}

uint32_t Cache::getReplacementBlockId(uint32_t begin, uint32_t end) {
  //Find dead 
  for (uint32_t i = begin; i < end; ++i) {
    if (this->blocks[i].dead)
      return i;
  }
  // Find invalid block first
  for (uint32_t i = begin; i < end; ++i) {
    if (!this->blocks[i].valid)
      return i;
  }

  // Otherwise use LRU
  uint32_t resultId = begin;
  uint32_t min = this->blocks[begin].lastReference;
  for (uint32_t i = begin; i < end; ++i) {
    if (this->blocks[i].lastReference < min) {
      resultId = i;
      min = this->blocks[i].lastReference;
    }
  }
  return resultId;
}

void Cache::writeBlockToLowerLevel(Cache::Block &b) {
  uint32_t addrBegin = this->getAddr(b);
  if (this->lowerCache == nullptr) {
    for (uint32_t i = 0; i < b.size; ++i) {
      this->memory->setByteNoCache(addrBegin + i, b.data[i]);
    }
  } else {
    for (uint32_t i = 0; i < b.size; ++i) {
      this->lowerCache->setByte(addrBegin + i, b.data[i]);
    }
  }
}

bool Cache::isPowerOfTwo(uint32_t n) { return n > 0 && (n & (n - 1)) == 0; }

uint32_t Cache::log2i(uint32_t val) {
  if (val == 0)
    return uint32_t(-1);
  if (val == 1)
    return 0;
  uint32_t ret = 0;
  while (val > 1) {
    val >>= 1;
    ret++;
  }
  return ret;
}

uint32_t Cache::getTag(uint32_t addr) {
  uint32_t offsetBits = log2i(policy.blockSize);
  uint32_t idBits = log2i(policy.blockNum / policy.associativity);
  uint32_t mask = (1 << (32 - offsetBits - idBits)) - 1;
  return (addr >> (offsetBits + idBits)) & mask;
}

uint32_t Cache::getId(uint32_t addr) {
  uint32_t offsetBits = log2i(policy.blockSize);
  uint32_t idBits = log2i(policy.blockNum / policy.associativity);
  uint32_t mask = (1 << idBits) - 1;
  return (addr >> offsetBits) & mask;
}

uint32_t Cache::getOffset(uint32_t addr) {
  uint32_t bits = log2i(policy.blockSize);
  uint32_t mask = (1 << bits) - 1;
  return addr & mask;
}

uint32_t Cache::getAddr(Cache::Block &b) {
  uint32_t offsetBits = log2i(policy.blockSize);
  uint32_t idBits = log2i(policy.blockNum / policy.associativity);
  return (b.tag << (offsetBits + idBits)) | (b.id << offsetBits);
}

/* Update the sampler when a cache  */
void Cache::Sampler::updateTraceWhenAccess(uint32_t setId,uint32_t tag){
  this->referenceCycle++;
  uint32_t blockId;
  tag &=0x7fff; // tag only have 15 bits
  // if it is in the set
  for(uint32_t i=setId;i<(setId+1)*this->associativity;i++){
    if(this->entries[i].tag == tag && this->entries[i].valid){
      this->entries[i].trace = (this->entries[i].trace+memPC)&(0x7fff);
      this->entries[i].lastReference = this->referenceCycle;
      return;
    }
  }
  // if there is a invalid line
  for(uint32_t i=setId;i<(setId+1)*this->associativity;i++){
    if(this->entries[i].valid!=1){
      this->entries[i].valid=1;
      this->entries[i].trace = memPC&(0x7fff);
      this->entries[i].tag = tag&(0x7fff);
      this->entries[i].lastReference = this->referenceCycle;
      return;
    }
  }
  // find LRU
  uint32_t min = this->entries[setId].lastReference;
  uint32_t resultId = setId;
  for(uint32_t i=setId;i<(setId+1)*this->associativity;i++){
    if(this->entries[i].lastReference<min){
      resultId = i;
    }
  }
  this->entries[resultId].lastReference = this->referenceCycle;
  this->entries[resultId].trace = memPC&(0x7fff);
  this->entries[resultId].tag = tag&(0x7fff);
  return;
}

/* Update predictor when replacement happens */
void Cache::Sampler::updatePredictorWhenReplace(uint32_t setId,uint32_t tag){
  uint32_t blockId;
  for(uint32_t i=setId;i<(setId+1)*this->associativity;i++){
    if(this->entries[i].tag == tag && this->entries[i].valid){
      uint32_t idx1 = this->hash1(this->entries[i].trace,tag)%4096;
      uint32_t idx2 = this->hash2(this->entries[i].trace,tag)%4096;
      uint32_t idx3 = this->hash3(this->entries[i].trace,tag)%4096;
      //update three tables
      this->predictor.updateCounter(idx1,idx2,idx3);
      this->entries[i].valid = 0;
      return;
    }
  }
}

uint32_t Cache::Sampler::hash1(uint32_t trace,uint32_t tag){
  return ((trace&0x7fff)^(tag&0x7fff))%4096;
}

uint32_t Cache::Sampler::hash2(uint32_t trace,uint32_t tag){
  return ((trace&0x7fff)+(tag&0x7fff))%4096;
}

uint32_t Cache::Sampler::hash3(uint32_t trace,uint32_t tag){
  return ((trace&0x7fff)&(tag&0x7fff))%4096;
}

void Cache::Sampler::Predictor::updateCounter(uint32_t idx1,uint32_t idx2,uint32_t idx3){
  if(this->counter1[idx1] < 3) counter1[idx1]++;
  if(this->counter2[idx2] < 3) counter2[idx2]++;
  if(this->counter3[idx3] < 3) counter3[idx3]++;
}

uint8_t Cache::Sampler::prediction(uint32_t pc,uint32_t tag){
  uint32_t idx1 = this->hash1(pc&0x7fff,tag&0x7fff)%4096;
  uint32_t idx2 = this->hash2(pc&0x7fff,tag&0x7fff)%4096;
  uint32_t idx3 = this->hash3(pc&0x7fff,tag&0x7fff)%4096;
  uint8_t confidence = this->predictor.counter1[idx1]+this->predictor.counter2[idx2]+this->predictor.counter3[idx3];
  if (confidence>=8){
    return 1;
  }
  return 0;
}