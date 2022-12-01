#ifndef DIRECTORY_MANAGER_H
#define DIRECTORY_MANAGER_H

#include <vector>
#include "Cache.h"
#include "MemoryManager.h"
#include "State.h"

class MemoryManager;
class Cache;
struct Block;

class DirectoryManager{
public:
  enum OwnerType{
    None = 0,
    Core0=1,
    Core1 =2,
    Llc = 3,
    Both = 4
  };
  struct DirectoryEntry{
    DirectoryEntry():state(),owner(None){};
    State state;
    OwnerType owner;
  };

  DirectoryManager():startAddr((uint32_t)-1),size(0){
    directoryTable.resize(0);
  }
  DirectoryManager(uint32_t startAddr, uint32_t size, uint32_t blockSize){
    this->startAddr = startAddr;
    this->size = size;
    this->directoryTable.resize(size/blockSize);
  }

  void handleLLCReadHit(uint32_t addr,Block*b,Block*c);
  void handleLLCReadMiss(uint32_t addr, Block* b);
  void hanldePrivateInvalid(uint32_t addr,State state);
  void handlePrivateWriteHit(uint32_t addr,Block* b);
  bool inDirectory(uint32_t addr);
  int getDirectoryId(uint32_t addr);

  std::vector<DirectoryEntry> directoryTable;
  uint32_t startAddr;
  uint32_t size;
  uint32_t blockSize;
  Cache * LLC;
  Cache * core0Cache;
  Cache * core1Cache;
  MemoryManager * memory;
};

#endif