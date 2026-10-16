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

#define NBUCKET 13

struct {
  struct spinlock lock[NBUCKET];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  // Create linked list of buffers
  for (int i=0;i<NBUCKET;i++){
    initlock(&bcache.lock[i],"bcache");
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    b->timestamp = 0;
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int id = blockno % NBUCKET;

  acquire(&bcache.lock[id]);

  // Is the block already cached?
  for(b = bcache.head[id].next; b != &bcache.head[id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  //找本桶中最旧的块
  uint64 oldest=~0ULL;
  struct buf *victim=0;
  for(b = bcache.head[id].next; b != &bcache.head[id]; b = b->next){
    if(b->refcnt == 0 && b->timestamp < oldest){
      oldest = b->timestamp;
      victim = b;
    }
  }
  if(victim){
    victim->dev = dev;
    victim->blockno = blockno;
    victim->valid = 0;
    victim->refcnt = 1;
    release(&bcache.lock[id]);
    acquiresleep(&victim->lock);
    return victim;
  }
  release(&bcache.lock[id]);

  //本桶满了找其它桶
  for(int i = 0; i < NBUCKET; i++){
    if(i == id) continue;

    acquire(&bcache.lock[i]);
    oldest = ~0ULL; victim = 0;
    for(b = bcache.head[i].next; b != &bcache.head[i]; b = b->next){
      if(b->refcnt == 0 && b->timestamp < oldest){
        oldest = b->timestamp;
        victim = b;
      }
    }
    if(victim){

      victim->prev->next = victim->next;
      victim->next->prev = victim->prev;
      release(&bcache.lock[i]);


      acquire(&bcache.lock[id]);
      victim->next = bcache.head[id].next;
      victim->prev = &bcache.head[id];
      bcache.head[id].next->prev = victim;
      bcache.head[id].next = victim;

      victim->dev = dev;
      victim->blockno = blockno;
      victim->valid = 0;
      victim->refcnt = 1;
      release(&bcache.lock[id]);
      acquiresleep(&victim->lock);
      return victim;
    }
    release(&bcache.lock[i]);
  }  

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // for(b = bcache.head[id].prev; b != &bcache.head[id]; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
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

  int id = b-> blockno % NBUCKET;
  acquire(&bcache.lock[id]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // b->next->prev = b->prev;
    // b->prev->next = b->next;
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
    b->timestamp=ticks;
  }
  
  release(&bcache.lock[id]);
}

void
bpin(struct buf *b) {
  int id = b->blockno % NBUCKET;
  acquire(&bcache.lock[id]);
  b->refcnt++;
  release(&bcache.lock[id]);
}

void
bunpin(struct buf *b) {
  int id = b->blockno % NBUCKET;
  acquire(&bcache.lock[id]);
  b->refcnt--;
  release(&bcache.lock[id]);
}


