/*
 * The main entry point of single level cache simulator
 * It takes a memory trace as input, and output CSV file containing miss rate
 * under various cache configurations
 *
 * Created by He, Hao at 2019-04-27
 */

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Cache.h"
#include "Debug.h"
#include "MemoryManager.h"

bool parseParameters(int argc, char **argv);
void printUsage();
// void simulateCache(std::ofstream &csvFile, uint32_t cacheSize,
//                    uint32_t blockSize, uint32_t associativity, bool writeBack,
//                    bool writeAllocate);

void simulateCache(std::ofstream &csvFile, bool writeBack);

bool verbose = false;
bool isSingleStep = false;
const char *traceFilePath;

int main(int argc, char **argv) {
  if (!parseParameters(argc, argv)) {
    return -1;
  }

  // Open CSV file and write header
  std::ofstream csvFile(std::string(traceFilePath) + ".csv");
  csvFile << "cacheSize,blockSize,associativity,writeBack,writeAllocate,"
             "missRate,totalCycles\n";

  // // Cache Size: 32 Kb to 32 Mb
  // for (uint32_t cacheSize = 32 * 1024; cacheSize <= 32 * 1024 * 1024;
  //      cacheSize *= 2) {
  //   // Block Size: 1 byte to 4096 byte
  //   // The maximum block size is imposed by VM page size
  //   for (uint32_t blockSize = 1; blockSize <= 4096; blockSize *= 2) {
  //     for (uint32_t associativity = 1; associativity <= 32;
  //          associativity *= 2) {
  //       uint32_t blockNum = cacheSize / blockSize;
  //       if (blockNum % associativity != 0)
  //         continue;

        // simulateCache(csvFile, cacheSize, blockSize, associativity, true, true);
        // simulateCache(csvFile, cacheSize, blockSize, associativity, true, false);
        // simulateCache(csvFile, cacheSize, blockSize, associativity, false, true);
        // simulateCache(csvFile, cacheSize, blockSize, associativity, false, false);
  //     }
  //   }
  // }
  simulateCache(csvFile,true);
  simulateCache(csvFile,false);
  printf("Result has been written to %s\n",
         (std::string(traceFilePath) + ".csv").c_str());
  csvFile.close();
  return 0;
}

bool parseParameters(int argc, char **argv) {
  // Read Parameters
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      switch (argv[i][1]) {
      case 'v':
        verbose = 1;
        break;
      case 's':
        isSingleStep = 1;
        break;
      default:
        return false;
      }
    } else {
      if (traceFilePath == nullptr) {
        traceFilePath = argv[i];
      } else {
        return false;
      }
    }
  }
  if (traceFilePath == nullptr) {
    return false;
  }
  return true;
}

void printUsage() {
  printf("Usage: CacheSim trace-file [-s] [-v]\n");
  printf("Parameters: -s single step, -v verbose output\n");
}

MemoryManager *memory;
Cache *l1Cache, *l2Cache, *l3Cache;
void simulateCache(std::ofstream &csvFile, bool writeBack) {
  Cache::Policy l1Policy, l2Policy, l3Policy;

  l1Policy.cacheSize = 32 * 1024;
  l1Policy.blockSize = 64;
  l1Policy.blockNum = l1Policy.cacheSize / l1Policy.blockSize;
  l1Policy.associativity = 8;
  l1Policy.hitLatency = 0;
  l1Policy.missLatency = 8;

  l2Policy.cacheSize = 256 * 1024;
  l2Policy.blockSize = 64;
  l2Policy.blockNum = l2Policy.cacheSize / l2Policy.blockSize;
  l2Policy.associativity = 8;
  l2Policy.hitLatency = 8;
  l2Policy.missLatency = 20;

  l3Policy.cacheSize = 8 * 1024 * 1024;
  l3Policy.blockSize = 64;
  l3Policy.blockNum = l3Policy.cacheSize / l3Policy.blockSize;
  l3Policy.associativity = 8;
  l3Policy.hitLatency = 20;
  l3Policy.missLatency = 100;



  // Initialize memory and cache
  memory = new MemoryManager();


  l3Cache = new Cache(memory, l3Policy,nullptr,writeBack);
  l2Cache = new Cache(memory, l2Policy, l3Cache,writeBack);
  l1Cache = new Cache(memory, l1Policy, l2Cache,writeBack);
  memory->setCache(l1Cache);
  l1Cache->printInfo(false);
  l2Cache->printInfo(false);
  l3Cache->printInfo(false);


  // Read and execute trace in cache-trace/ folder
  std::ifstream trace(traceFilePath);
  if (!trace.is_open()) {
    printf("Unable to open file %s\n", traceFilePath);
    exit(-1);
  }
  int32_t sum=0;
  char type; //'r' for read, 'w' for write
  uint32_t addr;
  while (trace >> type >> std::hex >> addr) {
    if (verbose)
      printf("%c %x\n", type, addr);
    if (!memory->isPageExist(addr))
      memory->addPage(addr);
    switch (type) {
    case 'r':
      memory->getByte(addr);
      break;
    case 'w':
      memory->setByte(addr, 0);
      break;
    default:
      dbgprintf("Illegal type %c\n", type);
      exit(-1);
    }

    sum++;
    if (verbose)
      l1Cache->printInfo(true);

    if (isSingleStep) {
      printf("Press Enter to Continue...");
      getchar();
    }
  }

  // Output Simulation Results
  memory->printStatistics();
  printf("==================sum=%d=================================\n",sum);
  // float missRate = (float)cache->statistics.numMiss /
  //                  (cache->statistics.numHit + cache->statistics.numMiss);
  // csvFile << cacheSize << "," << blockSize << "," << associativity << ","
  //         << writeBack << "," << writeAllocate << "," << missRate << ","
  //         << cache->statistics.totalCycles << std::endl;

  delete l1Cache;
  delete l2Cache;
  delete l3Cache;
  delete memory;
}