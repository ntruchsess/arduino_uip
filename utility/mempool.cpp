#include "mempool.h"
#include <string.h>

#define POOLOFFSET 1

MemoryPool::MemoryPool(memaddress start, memaddress size)
{
  memset(&blocks[0], 0, sizeof(blocks));
  blocks[POOLSTART].begin = start;
  blocks[POOLSTART].size = 0;
  blocks[POOLSTART].nextblock = NOBLOCK;
  poolsize = size;
}

memhandle
MemoryPool::allocBlock(memaddress size)
{
  memblock* best = NULL;
  memhandle cur = POOLSTART;
  memblock* block = &blocks[POOLSTART];
  memaddress bestsize = poolsize + 1;

  do
    {
      memhandle next = block->nextblock;
      memaddress freesize = ( next == NOBLOCK ? blocks[POOLSTART].begin + poolsize : blocks[next].begin) - block->begin - block->size;
      if (freesize == size)
        {
          best = &blocks[cur];
          goto found;
        }
      if (freesize > size && freesize < bestsize)
        {
          bestsize = freesize;
          best = &blocks[cur];
        }
      if (next == NOBLOCK)
        {
          if (best)
            goto found;
          else
            goto collect;
        }
      block = &blocks[next];
      cur = next;
    }
  while (true);

  collect:
    {
      cur = POOLSTART;
      block = &blocks[POOLSTART];
      memhandle next;
      while ((next = block->nextblock) != NOBLOCK)
        {
          memaddress dest = block->begin + block->size;
          memblock* nextblock = &blocks[next];
          memaddress* src = &nextblock->begin;
          if (dest != *src)
            {
#ifdef MEMBLOCK_MV
              memblock_mv_cb(dest,*src,nextblock->size);
#endif
              *src = dest;
            }
          block = nextblock;
        }
      if (blocks[POOLSTART].begin + poolsize - block->begin - block->size >= size)
        best = block;
      else
        goto notfound;
    }

  found:
    {
      block = &blocks[POOLOFFSET];
      for (cur = POOLOFFSET; cur < NUM_MEMBLOCKS + POOLOFFSET; cur++)
        {
          if (block->size)
            {
              block++;
              continue;
            }
          memaddress address = best->begin + best->size;
#ifdef MEMBLOCK_ALLOC
          MEMBLOCK_ALLOC(address,size);
#endif
          block->begin = address;
          block->size = size;
          block->nextblock = best->nextblock;
          best->nextblock = cur;
          return cur;
        }
    }

  notfound: return NOBLOCK;
}

void
MemoryPool::freeBlock(memhandle handle)
{
  memblock *b = &blocks[POOLSTART];

  do
    {
      memhandle next = b->nextblock;
      if (next == handle)
        {
          memblock *f = &blocks[next];
#ifdef MEMBLOCK_FREE
          MEMBLOCK_FREE(f->begin,f->size);
#endif
          b->nextblock = f->nextblock;
          f->size = 0;
          f->nextblock = NOBLOCK;
          return;
        }
      if (next == NOBLOCK)
        return;
      b = &blocks[next];
    }
  while (true);
}

void
MemoryPool::resizeBlock(memhandle handle, memaddress position)
{
  memblock * block = &blocks[handle];
  block->begin += position;
  block->size -= position;
}

void
MemoryPool::resizeBlock(memhandle handle, memaddress position, memaddress size)
{
  memblock * block = &blocks[handle];
  block->begin += position;
  block->size = size;
}

memaddress
MemoryPool::blockSize(memhandle handle)
{
  return blocks[handle].size;
}
