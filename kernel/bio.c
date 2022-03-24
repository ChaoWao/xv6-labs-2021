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

// There are mainly two methods here.
// The first is to split buf, each bucket has
// its own bufs.
// The second is like the first only when 
// its own bufs has no not used buf. It can 
// steal from others.
// The second is implemented here.

struct bucket {
  struct spinlock lock;
  struct buf head;
};
struct {
  struct spinlock lock;
  struct buf buf[NBUF];
#define N_BUCKETS 13
  struct bucket buckets[N_BUCKETS];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

uint hash(uint blockno) {
  return blockno % N_BUCKETS;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // init each bucket
  for (int i = 0; i < N_BUCKETS; i++) {
    initlock(&bcache.buckets[i].lock, "bcache");
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
  }

  // Create linked list of buffers
  // since for now, blockno is all zero, 
  // they are linked to bucket 0 

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.buckets[0].head.next;
    b->prev = &bcache.buckets[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].head.next->prev = b;
    bcache.buckets[0].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint bucket_index = hash(blockno);

  //acquire(&bcache.lock);

  acquire(&bcache.buckets[bucket_index].lock);

  // Is the block already cached?
  for(b = bcache.buckets[bucket_index].head.next; b != &bcache.buckets[bucket_index].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.buckets[bucket_index].lock);
      //release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Fisrt 
  // Recycle the least recently used (LRU) unused buffer.
  // of its own list
  struct buf *unused = (struct buf *)0;
  for(b = bcache.buckets[bucket_index].head.prev; b != &bcache.buckets[bucket_index].head; b = b->prev){
    if(b->refcnt == 0) {
      if (!unused || unused->ticks > b->ticks) {
        unused = b;
      }
    }
  }
  if (unused) {
    unused->dev = dev;
    unused->blockno = blockno;
    unused->valid = 0;
    unused->refcnt = 1;
    unused->ticks = ticks;
    release(&bcache.buckets[bucket_index].lock);
    //release(&bcache.lock);
    acquiresleep(&unused->lock);
    return unused;
  }
  // Second
  // Recycle the least recently used (LRU) unused buffer.
  // of others list
  // to aviod deadlock, scan with order bucket 0 -> N_BUCKETS
  int i;
  // if (bucket_index == 0) {
  //   for (i = 1; i < N_BUCKETS; i++) {
  //     acquire(&bcache.buckets[i].lock);
  //     for(b = bcache.buckets[i].head.prev; b != &bcache.buckets[i].head; b = b->prev){
  //       if(b->refcnt == 0) {
  //         if (!unused || unused->ticks > b->ticks) {
  //           unused = b;
  //         }
  //       }
  //     }
  //     if (unused) 
  //       break;
  //     else
  //       release(&bcache.buckets[i].lock);
  //   }
  // } else {

  // release bigger bucket_index so to acquire smaller lock
  release(&bcache.buckets[bucket_index].lock);
  for (i = 0; i < N_BUCKETS; i++) {
    if (i == bucket_index) {
      acquire(&bcache.buckets[bucket_index].lock);
    }
    else {
      acquire(&bcache.buckets[i].lock);
      for(b = bcache.buckets[i].head.prev; b != &bcache.buckets[i].head; b = b->prev){
        if(b->refcnt == 0) {
          if (!unused || unused->ticks > b->ticks) {
            unused = b;
          }
        }
      }
      if (unused) {
        if (i < bucket_index) {
          acquire(&bcache.buckets[bucket_index].lock);
        }
        break;
      } else {
        release(&bcache.buckets[i].lock);
      }
    }
  }
  // }
  if (unused) {
    // remove from old bucket
    unused->prev->next = unused->next;
    unused->next->prev = unused->prev;
    // reuse
    unused->dev = dev;
    unused->blockno = blockno;
    unused->valid = 0;
    unused->refcnt = 1;
    unused->ticks = ticks;
    // add to new bucket
    unused->next = bcache.buckets[bucket_index].head.next;
    unused->prev = &bcache.buckets[bucket_index].head;
    bcache.buckets[bucket_index].head.next->prev = unused;
    bcache.buckets[bucket_index].head.next = unused;
    // release locks
    release(&bcache.buckets[bucket_index].lock);
    release(&bcache.buckets[i].lock);
    //release(&bcache.lock);
    acquiresleep(&unused->lock);
    return unused;
  }
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

  //acquire(&bcache.lock);
  uint bucket_index = hash(b->blockno);
  acquire(&bcache.buckets[bucket_index].lock);

  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    //b->next->prev = b->prev;
    //b->prev->next = b->next;
    //b->next = bcache.head.next;
    //b->prev = &bcache.head;
    //bcache.head.next->prev = b;
    //bcache.head.next = b;
    b->ticks = ticks;
  }
  
  release(&bcache.buckets[bucket_index].lock);
  //release(&bcache.lock);
}

void
bpin(struct buf *b) {
  //acquire(&bcache.lock);
  uint id = hash(b->blockno);
  acquire(&bcache.buckets[id].lock);
  b->refcnt++;
  release(&bcache.buckets[id].lock);
  //release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  //acquire(&bcache.lock);
  uint id = hash(b->blockno);
  acquire(&bcache.buckets[id].lock);
  b->refcnt--;
  release(&bcache.buckets[id].lock);
  //release(&bcache.lock);
}


