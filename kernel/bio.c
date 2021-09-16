// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define prime 13

struct {
  struct spinlock lock[prime];
  struct buf buf[prime][2]; // NBUF is 30
} bcache;

void
binit(void)
{
  struct buf *b;

  for (int i = 0; i < prime; i++) {
    // printf("bcache: %d\n", i);
    initlock(&bcache.lock[i], "bcache.bucket" + i);
  }

  for (int i = 0; i < prime; i++) {
  // Create linked list of buffers
    for(b = bcache.buf[i]; b < bcache.buf[i] + 2; b++){
      // printf("buffer: %d\n", i);
      initsleeplock(&b->lock, "buffer" + i);
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int bucket = blockno % prime;
  acquire(&bcache.lock[bucket]);

  // Is the block already cached?
  for(b = bcache.buf[bucket]; b < bcache.buf[bucket] + NBUF; b++){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      acquire(&tickslock);
      b->ticks = ticks;    // use ticks instead of link-list for LRU
      release(&tickslock);
      release(&bcache.lock[bucket]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  struct buf * temp = bcache.buf[bucket];
  for(b = bcache.buf[bucket]; b < bcache.buf[bucket] + NBUF; b++){
    if(b->ticks < temp->ticks) {
      temp = b;
    }
  }
  temp->dev = dev;
  temp->blockno = blockno;
  temp->valid = 0;
  temp->refcnt = 1;
  acquire(&tickslock);
  temp->ticks = ticks;    // use ticks instead of link-list for LRU
  release(&tickslock);
  release(&bcache.lock[bucket]);
  // printf("dev: %p, blockno: %p\n", temp->dev, temp->blockno);
  acquiresleep(&temp->lock);
  return temp;
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int bucket = b->blockno % prime;
  acquire(&bcache.lock[bucket]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->ticks = 0;
  }
  release(&bcache.lock[bucket]);
}

void
bpin(struct buf *b) {
  int bucket = b->blockno % prime;
  acquire(&bcache.lock[bucket]);
  b->refcnt++;
  release(&bcache.lock[bucket]);
}

void
bunpin(struct buf *b) {
  int bucket = b->blockno % prime;
  acquire(&bcache.lock[bucket]);
  b->refcnt--;
  release(&bcache.lock[bucket]);
}


