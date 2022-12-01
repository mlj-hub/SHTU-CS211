/*
 * Created by He, Hao at 2019-3-11
 */

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <pthread.h>

#include <elfio/elfio.hpp>

#include "BranchPredictor.h"
#include "Cache.h"
#include "Debug.h"
#include "MemoryManager.h"
#include "Simulator.h"
#include "DirectoryManager.h"

bool parseParameters(int argc, char **argv);
void printUsage();
void printElfInfo(ELFIO::elfio *reader);
void loadElfToMemory(ELFIO::elfio *reader, MemoryManager *memory,uint32_t base);
void * runCore0(void *);
void * runCore1(void *);

char *elfFile1 = nullptr;
char *elfFile2 = nullptr;
bool verbose = 0;
bool isSingleStep = 0;
bool dumpHistory = 0;
uint32_t stackBaseAddrCore0 = 0xa0000000;
uint32_t stackBaseAddrCore1 = 0x90000000;
uint32_t stackSize = 0x400000;
MemoryManager memory;
Cache *l1Cache1, *l2Cache1, *l1Cache2, *l2Cache2, *l3Cache;
BranchPredictor::Strategy strategy = BranchPredictor::Strategy::NT;
BranchPredictor branchPredictor;
uint32_t sharedBase = 0x100000, sharedSize = 0x400000;
uint32_t core0Base=0x800000, core1Base=0x40800000;
Simulator simulator0(&memory, &branchPredictor,core0Base,sharedBase,sharedSize);
Simulator simulator1(&memory, &branchPredictor,core1Base,sharedBase,sharedSize);
pthread_t core0, core1;
DirectoryManager directory0(sharedBase,0x200000,64),directory1(0x300000,0x200000,64);
pthread_mutexattr_t attr;
pthread_mutex_t LLCLock;

int main(int argc, char **argv) {
  if (!parseParameters(argc, argv)) {
    printUsage();
    exit(-1);
  }

  // Init cache
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

  pthread_mutexattr_init(&attr);
  pthread_mutex_init(&LLCLock,&attr);

  l3Cache = new Cache(&memory, l3Policy,3,nullptr,&LLCLock);

  l2Cache1 = new Cache(&memory, l2Policy, 2,l3Cache);
  l1Cache1 = new Cache(&memory, l1Policy, 1,l2Cache1);

  l2Cache2 = new Cache(&memory,l2Policy,2,l3Cache);
  l1Cache2 = new Cache(&memory,l1Policy,1,l2Cache2);


  memory.setCache(l1Cache1,l1Cache2);
  memory.initShared(sharedBase,sharedSize);

  directory0.core0Cache = l1Cache1;
  directory0.core1Cache = l1Cache2;
  directory0.LLC = l3Cache;
  directory0.memory = &memory;

  directory1.core0Cache = l1Cache1;
  directory1.core1Cache = l1Cache2;
  directory1.LLC = l3Cache;
  directory1.memory = &memory;

  memory.directory0 = &directory0;
  memory.directory1 = &directory1;

  // Read ELF file
  ELFIO::elfio reader0;
  ELFIO::elfio reader1;
  if(elfFile1 && !reader0.load(elfFile1)){
    fprintf(stderr, "Fail to load ELF file %s!\n", elfFile1);
    return -1;
  }
  if (elfFile2 &&!reader1.load(elfFile2)) {
    fprintf(stderr, "Fail to load ELF file %s!\n", elfFile1);
    return -1;
  }

  if (verbose) {
    printElfInfo(&reader0);
    printElfInfo(&reader1);
  }
  if(elfFile1)
    loadElfToMemory(&reader0, &memory,core0Base);
  if(elfFile2)
    loadElfToMemory(&reader1, &memory,core1Base);
  if (verbose) {
    memory.printInfo();
  }

  simulator0.isSingleStep = isSingleStep;
  simulator0.verbose = verbose;
  simulator0.shouldDumpHistory = dumpHistory;
  simulator0.branchPredictor->strategy = strategy;
  simulator0.core = core0;
  simulator0.pc = reader0.get_entry();
  simulator0.initStack(stackBaseAddrCore0, stackSize);

  simulator1.isSingleStep = isSingleStep;
  simulator1.verbose = verbose;
  simulator1.shouldDumpHistory = dumpHistory;
  simulator1.branchPredictor->strategy = strategy;
  simulator1.core = core1;
  simulator1.pc = reader1.get_entry();
  simulator1.initStack(stackBaseAddrCore1, stackSize);

  if(elfFile1){
    int successCore0 = pthread_create(&core0,NULL,runCore0,NULL);
    if(successCore0)
      printf("core0 fails\n");
  }
  if(elfFile2){
    int successCore1 = pthread_create(&core1,NULL,runCore1,NULL);
    if(successCore1)
      printf("core1 fails\n");
  }

  if(elfFile1)
    pthread_join(core0,NULL);
  if(elfFile2)
    pthread_join(core1,NULL);

  if (dumpHistory) {
    printf("Dumping history to dump.txt...\n");
    simulator0.dumpHistory();
    simulator1.dumpHistory();
  }

  delete l1Cache1;
  delete l2Cache1;
  delete l1Cache2;
  delete l2Cache2;
  delete l3Cache;
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
      case 'd':
        dumpHistory = 1;
        break;
      case 'b':
        if (i + 1 < argc) {
          std::string str = argv[i + 1];
          i++;
          if (str == "AT") {
            strategy = BranchPredictor::Strategy::AT;
          } else if (str == "NT") {
            strategy = BranchPredictor::Strategy::NT;
          } else if (str == "BTFNT") {
            strategy = BranchPredictor::Strategy::BTFNT;
          } else if (str == "BPB") {
            strategy = BranchPredictor::Strategy::BPB;
          } else {
            return false;
          }
        } else {
          return false;
        }
        break;
      case 'c':
        if(argv[i][2]=='0'){
          if(elfFile1==nullptr)
            elfFile1 = argv[++i];
          else
            return false;
        }
        else if(argv[i][2]=='1'){
          if(elfFile2==nullptr)
            elfFile2 = argv[++i];
          else
            return false;
        }
        else{
          return false;
        }
        break;
      default:
        return false;
      }
    }
  }
  if (elfFile1 == nullptr && elfFile2 == nullptr) {
    return false;
  }
  return true;
}

void printUsage() {
  printf("Usage: Simulator riscv-elf-file [-v] [-s] [-d] [-b param]\n");
  printf("Parameters: \n\t[-v] verbose output \n\t[-s] single step\n");
  printf("\t[-d] dump memory and register trace to dump.txt\n");
  printf("\t[-b param] branch perdiction strategy, accepted param AT, NT, "
         "BTFNT, BPB\n");
}

void printElfInfo(ELFIO::elfio *reader) {
  printf("==========ELF Information==========\n");

  if (reader->get_class() == ELFCLASS32) {
    printf("Type: ELF32\n");
  } else {
    printf("Type: ELF64\n");
  }

  if (reader->get_encoding() == ELFDATA2LSB) {
    printf("Encoding: Little Endian\n");
  } else {
    printf("Encoding: Large Endian\n");
  }

  if (reader->get_machine() == EM_RISCV) {
    printf("ISA: RISC-V(0x%x)\n", reader->get_machine());
  } else {
    dbgprintf("ISA: Unsupported(0x%x)\n", reader->get_machine());
    exit(-1);
  }

  ELFIO::Elf_Half sec_num = reader->sections.size();
  printf("Number of Sections: %d\n", sec_num);
  printf("ID\tName\t\tAddress\tSize\n");

  for (int i = 0; i < sec_num; ++i) {
    const ELFIO::section *psec = reader->sections[i];

    printf("[%d]\t%-12s\t0x%lx\t%ld\n", i, psec->get_name().c_str(),
           psec->get_address(), psec->get_size());
  }

  ELFIO::Elf_Half seg_num = reader->segments.size();
  printf("Number of Segments: %d\n", seg_num);
  printf("ID\tFlags\tAddress\tFSize\tMSize\n");

  for (int i = 0; i < seg_num; ++i) {
    const ELFIO::segment *pseg = reader->segments[i];

    printf("[%d]\t0x%x\t0x%lx\t%ld\t%ld\n", i, pseg->get_flags(),
           pseg->get_virtual_address(), pseg->get_file_size(),
           pseg->get_memory_size());
  }

  printf("===================================\n");
}

void loadElfToMemory(ELFIO::elfio *reader, MemoryManager *memory,uint32_t base) {
  ELFIO::Elf_Half seg_num = reader->segments.size();
  for (int i = 0; i < seg_num; ++i) {
    const ELFIO::segment *pseg = reader->segments[i];

    uint64_t fullmemsz = pseg->get_memory_size();
    uint64_t fulladdr = pseg->get_virtual_address();
    // Our 32bit simulator cannot handle this
    if (fulladdr + fullmemsz > 0xFFFFFFFF) {
      dbgprintf(
          "ELF address space larger than 32bit! Seg %d has max addr of 0x%lx\n",
          i, fulladdr + fullmemsz);
      exit(-1);
    }

    uint32_t filesz = pseg->get_file_size();
    uint32_t memsz = pseg->get_memory_size();
    uint32_t addr = (uint32_t)pseg->get_virtual_address();

    for (uint32_t p = addr+base; p < addr + memsz+base; ++p) {
      if (!memory->isPageExist(p)) {
        memory->addPage(p);
      }

      if (p < addr + filesz+base) {
        memory->setByteNoCache(p, pseg->get_data()[p - addr-base]);
      } else {
        memory->setByteNoCache(p, 0);
      }
    }
  }
}

void * runCore0(void * _){
  simulator0.simulate();
  return NULL;
}

void * runCore1(void * _){
  simulator1.simulate();
  return NULL;
}