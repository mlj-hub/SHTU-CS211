/*
 * Implementation of a simple cache simulator
 *
 * Created By He, Hao in 2019-04-27
 */

#include <cstdio>
#include <cstdlib>

#include "Cache.h"
#include "State.h"

Cache::Cache(MemoryManager *manager, Policy policy,uint8_t level, Cache *lowerCache,pthread_mutex_t* lock,
             bool writeBack, bool writeAllocate) {
  this->referenceCounter = 0;
  this->memory = manager;
  this->policy = policy;
  this->lowerCache = lowerCache;
  this->level = level;
  this->lock = lock;
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
}

bool Cache::inCache(uint32_t addr) {
  return getBlockId(addr) != (uint32_t)-1 ? true : false;
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
  bool shared = this->inShared(addr);

  // If in cache, return directly
  int blockId;
  if ((blockId = this->getBlockId(addr)) != -1) {
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
  if(shared)
    this->loadSharedBlockFromLowerLevel(addr&(0xffffffff<<log2i(this->policy.blockSize)), cycles);
  else
    this->loadBlockFromLowerLevel(addr,cycles);

  // The block is in top level cache now, return directly
  if ((blockId = this->getBlockId(addr)) != -1) {
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
  bool shared = this->inShared(addr);

  // If in cache, write to it directly
  int blockId;
  if ((blockId = this->getBlockId(addr)) != -1) {
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

    if(this->lowerCache != nullptr && shared){
      this->blocks[blockId].state = State::Modified;
      if(this->level!=1){
        printf("==========\n");
      }
      this->memory->getDirectory(addr)->handlePrivateWriteHit(addr,&this->blocks[blockId]);
    }
    if (cycles) *cycles = this->policy.hitLatency;
    return;
  }

  // Else, load the data from cache
  // TODO: implement bypassing
  this->statistics.numMiss++;
  this->statistics.totalCycles += this->policy.missLatency;

  if (this->writeAllocate) {
    if(shared)
      this->loadSharedBlockFromLowerLevel(addr&(0xffffffff<<log2i(this->policy.blockSize)), cycles);
    else
      this->loadBlockFromLowerLevel(addr,cycles);
    
    blockId = this->getBlockId(addr);
    
    if(shared)
      this->memory->getDirectory(addr)->handlePrivateWriteHit(addr,&this->blocks[blockId]);

    if ( blockId!= -1) {
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
    for (size_t j = 0; j < this->blocks.size(); ++j) {
      const Block &b = this->blocks[j];
      printf("Block %ld: tag 0x%x id %d %s %s (last ref %d)\n", j, b.tag, b.id,
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
  printf("Total Cycles: %lu\n", this->statistics.totalCycles);
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
  b.state = State::Invalid;
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
  if (this->writeBack && replaceBlock.valid &&
      replaceBlock.modified) { // write back to memory
    this->writeBlockToLowerLevel(replaceBlock);
    this->statistics.totalCycles += this->policy.missLatency;
  }

  this->blocks[replaceId] = b;
}

uint32_t Cache::getReplacementBlockId(uint32_t begin, uint32_t end) {
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

void Cache::writeBlockToLowerLevel(Block &b) {
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

uint32_t Cache::getAddr(Block &b) {
  uint32_t offsetBits = log2i(policy.blockSize);
  uint32_t idBits = log2i(policy.blockNum / policy.associativity);
  return (b.tag << (offsetBits + idBits)) | (b.id << offsetBits);
}

bool Cache::inShared(uint32_t addr){
  return (addr<0x100000+0x400000 && addr >= 0x100000);
}

void Cache::setState(uint32_t addr, State state){
  uint32_t blockId = this->getBlockId(addr);
  if(blockId == (uint32_t)-1){
    fprintf(stderr, "cannot set state to a cache block not in cache \n");
    exit(-1);
  }
  this->blocks[blockId].state = state;
}

/* This function will load a cache block which contains addr into the calling cache. Also,
   it will maintain the corresponding state of cache block */
void Cache::loadSharedBlockFromLowerLevel(uint32_t addr, uint32_t * cycles){
  uint32_t blockSize = this->policy.blockSize;

  // Initialize new block from memory
  Block b;
  b.valid = true;
  b.modified = false;
  b.tag = this->getTag(addr);
  b.id = this->getId(addr);
  b.size = blockSize;
  b.data = std::vector<uint8_t>(b.size);

  if(this->lowerCache != nullptr){
    if(this->lowerCache->lowerCache == nullptr)
      pthread_mutex_lock(this->lowerCache->lock);
    this->lowerCache->passSharedBlockToHigherLevel(addr, &b,cycles);
    if(this->lowerCache->lowerCache == nullptr)
      pthread_mutex_unlock(this->lowerCache->lock);
  }
  else{
    // L3 read miss
    this->memory->getDirectory(addr)->handleLLCReadMiss(addr,&b);
  }

  // Find replace block
  uint32_t id = this->getId(addr);
  uint32_t blockIdBegin = id * this->policy.associativity;
  uint32_t blockIdEnd = (id + 1) * this->policy.associativity;
  uint32_t replaceId = this->getReplacementBlockId(blockIdBegin, blockIdEnd);
  Block replaceBlock = this->blocks[replaceId];
  uint32_t beginAddr = this->getAddr(replaceBlock);
  if(this->inShared(beginAddr)){
    if(this->lowerCache != nullptr){
      Block replaceBlock = this->blocks[replaceId];
      switch(replaceBlock.state){
        case State::Modified:
          this->handleWriteBack(replaceBlock);
          this->statistics.totalCycles += this->policy.missLatency;
          break;
        case State::Shared: case State::Exclusive:
          // the victim cache line is S or E, which means clean, first invalid it
          this->invalidCacheBlock(beginAddr);
          this->memory->getDirectory(beginAddr)->hanldePrivateInvalid(beginAddr,replaceBlock.state);
          break;
        case State::Invalid:
        default:
          break;
      }
    }
    else{
      if(this->blocks[replaceId].valid && this->blocks[replaceId].state == State::Modified)
        this->handleWriteBack(this->blocks[replaceId]);
      if(this->blocks[replaceId].valid && this->blocks[replaceId].state == State::Invalid){
        this->handleWriteBack(this->blocks[replaceId]);
      }
    }
  }
  else{
    if (this->writeBack && replaceBlock.valid &&
        replaceBlock.modified) { // write back to memory
      this->writeBlockToLowerLevel(replaceBlock);
      this->statistics.totalCycles += this->policy.missLatency;
    }
  }

  this->blocks[replaceId] = b;
}

/* This function will put the Cache Block which contains addr into b. If this cache 
   block is not in the cache, it will load it from the lower level and maintain the 
   coherence of the cache block */
void Cache::passSharedBlockToHigherLevel(uint32_t addr, Block * b, uint32_t * cycles){
  bool incache = this->inCache(addr);
  int id = this->getBlockId(addr);
  if(incache){
    if(this->lowerCache == nullptr){
      this->memory->getDirectory(addr)->handleLLCReadHit(addr,b,&this->blocks[id]);
    }
    else{
      this->handlePrivateReadHit(addr,b,&this->blocks[id]);
    }
    return;
  }
  else{
    this->loadSharedBlockFromLowerLevel(addr,cycles);
  }

  incache = this->inCache(addr);

  if(!incache){
    fprintf(stderr, "fail load\n");
    exit(-1);
  }

  int blockId = this->getBlockId(addr);
  b->data = this->blocks[blockId].data;
  b->state = this->blocks[blockId].state;
  // if the passed cache line is modifeid, invalid this cache line.
  if(this->blocks[blockId].state == State::Modified)
    this->blocks[blockId].valid = 0;
  return;
}

void Cache::findSetStateAll(uint32_t addr, State state,std::vector<uint8_t> &data){
  Cache * temp = this;
  int cnt = 0;
  while(temp!= nullptr){
    int id = temp->getBlockId(addr);
    if(id!=-1){
      cnt+=1;
      temp->blocks[id].state = state;
      data = temp->blocks[id].data;
    }
    temp = temp->lowerCache;
  }
}

void Cache::setStateAll(uint32_t addr, State state){
  Cache * temp = this;
  while(temp!= nullptr){
    int id = temp->getBlockId(addr);
    if(id!=-1){
      temp->blocks[id].state = state;
    }
    temp = temp->lowerCache;
  }
}

void Cache::handlePrivateReadHit(uint32_t addr,Block*dstB,Block*srcB){
  dstB->data = srcB->data;
  dstB->state = srcB->state;
  if(srcB->state == State::Modified){
    srcB->state = State::Invalid;
    srcB->valid = 0;
    srcB->modified = 0;
  }
}

// any private cache write back can be write back to the memory directly
// since no other copy can exist
void Cache::handleWriteBack(Block b){
  uint32_t addr = this->getAddr(b);
  for(uint32_t i=0;i<this->policy.blockSize;i++){
    this->memory->setByteNoCache(addr+i,b.data[i]);
  }
  DirectoryManager * directory = this->memory->getDirectory(addr);
  int id = directory->getDirectoryId(addr);
  directory->directoryTable[id].state = State::Invalid;
  directory->directoryTable[id].owner = DirectoryManager::OwnerType::None;
}

void Cache::invalidCacheBlock(uint32_t addr){
  int id = this->getBlockId(addr);
  if(id!=-1){
    this->blocks[id].valid = 0;
    this->blocks[id].modified = 0;
  }
}