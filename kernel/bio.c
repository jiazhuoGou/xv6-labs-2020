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

#define NBUFMAP_BUCKET 13
  #define BUFMAP_HASH(dev, blockno) ( (((dev)<<27)|(blockno))%NBUFMAP_BUCKET )

struct {
  struct spinlock eviction_lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;

  // <重新设计，hash: dev和blockno映射至buf
  struct buf bufmap[NBUFMAP_BUCKET];
  struct spinlock bufmap_locks[NBUFMAP_BUCKET];

} bcache;

void
binit(void)
{
  // <直接用单链表加哈希>
  for (int i = 0; i < NBUFMAP_BUCKET; ++i)
  { // <初始化bufmap>
    initlock(&bcache.bufmap_locks[i], "bcache_bufmap");
    bcache.bufmap[i].next = 0;
  }
  for (int i = 0; i < NBUF; ++i)
  { // 初始化缓存块
    struct buf *b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    b->lastuse = 0;
    b->refcnt = 0;
    b->next = bcache.bufmap[0].next;  // 尾插法
    bcache.bufmap[0].next = b;
  }
  initlock(&bcache.eviction_lock, "bache_eviction");

}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  //  <参照掘金的做法，在驱逐删除的时候退化成单线程>
  struct buf *b;
  uint key = BUFMAP_HASH(dev, blockno);
  // <先获取key桶的锁>
  acquire(&bcache.bufmap_locks[key]);

  // <判断是否命中>
  for (b = bcache.bufmap[key].next; b; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    { // 命中
      b->refcnt++;
      release(&bcache.bufmap_locks[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // <未命中，为了防止多个进程同时找一个cache造成的重复缓存的错误
  release(&bcache.bufmap_locks[key]);  // <先暂时放弃桶的锁
  acquire(&bcache.eviction_lock); // 驱逐删除的时候退化成单线程
  // <在重新扫一遍，因为在未获得这个锁之前，可能其他线程将这个cache加进来了>
  for (b = bcache.bufmap[key].next; b; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    { // 命中
      b->refcnt++;
      release(&bcache.bufmap_locks[key]);
      release(&bcache.eviction_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // <仍然没找到，要开始lru>
  struct buf * before_least = 0;
  uint holding_bucket = -1;
  for (int i = 0; i < NBUFMAP_BUCKET; ++i)
  {
    acquire(&bcache.bufmap_locks[i]); // 获取第i个的锁，因为有可能要eviction
    int newfound = 0; // <最近最少使用的缓冲区>
    for (b = &bcache.bufmap[i]; b->next; b = b->next)
    { // 从第i个桶开始
      if (b->next->refcnt == 0 && (!before_least || b->next->lastuse < before_least->next->lastuse))
      {
        before_least = b;
        newfound = 1;
      }
    }
    if (!newfound)
    { // 第i个桶里面都是才用的，说明要接着去找更久的
      release(&bcache.bufmap_locks[i]);
    }
    else
    { // 找到了一个最新的没用的
      // 如果还有更新的，释放掉之前获取的锁 
      if (holding_bucket != -1) release(&bcache.bufmap_locks[holding_bucket]);
      holding_bucket = i;
    }
  }

  if (!before_least)
    panic("bget: no buffers");  // 找了一圈都没有
  b = before_least->next;

  if (holding_bucket != key)
  { // 找到了一个新的cache，将它放入新的key桶里面，
    before_least->next = b->next;
    release(&bcache.bufmap_locks[holding_bucket]);
    acquire(&bcache.bufmap_locks[key]);
    b->next = bcache.bufmap[key].next;
    bcache.bufmap[key].next = b;
  }
  b->dev = dev;
  b->blockno = blockno;
  b->refcnt = 1;
  b->valid = 0;
  release(&bcache.bufmap_locks[key]);
  release(&bcache.eviction_lock);
  acquiresleep(&b->lock);
  return b;
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
  
  uint key = BUFMAP_HASH(b->dev, b->blockno);
  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->lastuse = ticks;
  }
  release(&bcache.bufmap_locks[key]);
}

void
bpin(struct buf *b) {
  uint key = BUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.bufmap_locks[key]);
  b->refcnt++;
  release(&bcache.bufmap_locks[key]);
}

void
bunpin(struct buf *b) {
  uint key = BUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  release(&bcache.bufmap_locks[key]);
}


