getByte
  --> if hit, just read the byte.
  --> if miss, call loadBlockFromLowerLevelE to get the data from the lower level
  --> get a cache block to be replaced 
  --> evict one cache block, if any, by calling writeBlockFromLowerLevelE
  --> read the byte

setByte
  --> if hit, just write the byte.
     --> if write through, write the memory
  --> if miss, call loadBlockFromLowerLevelE to get the data from the lower level
  --> get a cache block to be replaced
  --> evict one cache block, if any, by calling writeBlockFromLowerLevelE
  --> write the byte.
     --> if write through, write the memory

loadBlockFromLowerLevelE
  --> if the cache hit, return the data and mark it as invalid
  --> if miss, call loadBlockFromLowerLevelE to get the data from the lower level and return it.

writeBlockToLowerLevelE
  --> find a evictim cache block
  --> if not full, just write
  --> else, call writeBlockToLowerLevelE