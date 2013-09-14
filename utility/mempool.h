#ifndef MEMPOOL_H
#define MEMPOOL_H

#include <inttypes.h>

#define POOLSTART 0
#define NOBLOCK 0

#include "mempool_conf.h"

#ifdef MEMBLOCK_MV
#define memblock_mv_cb(dest,src,size) MEMBLOCK_MV(dest,src,size)
#endif

#ifdef MEMBLOCK_ALLOC
#define memblock_alloc_cb(address,size) MEMBLOCK_ALLOC(address,size)
#endif

#ifdef MEMBLOCK_FREE
#define memblock_free_cb(address,size) MEMBLOCK_FREE(address,size)
#endif

struct memblock
{
  memaddress begin;
  memaddress size;
  memhandle nextblock;
};

class MemoryPool
{
#ifdef MEMPOOLTEST_H
  friend class MemoryPoolTest;
#endif

  friend class Enc28J60Network;

private:
  memaddress poolsize;
  struct memblock blocks[NUM_MEMBLOCKS+1];

public:
  MemoryPool(memaddress start, memaddress size);
  memhandle
  allocBlock(memaddress);
  void freeBlock(memhandle);
};
#endif
