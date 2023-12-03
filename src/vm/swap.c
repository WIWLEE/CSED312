#include "vm/swap.h"
#include "devices/block.h"
#include <bitmap.h>
#include "threads/synch.h"

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

struct bitmap *swap_bitmap;

struct lock swap_lock;

void swap_init(){
  lock_init(&swap_lock);
  swap_bitmap = bitmap_create(8 * 1024); // 8 * 1024 만큼의 bitmap을 만든다.
}


//
void swap_in(size_t used_index, void* kaddr){
  struct block *swap_device;
  lock_acquire(&swap_lock);
  swap_device = block_get_role (BLOCK_SWAP);
  block_sector_t s = used_index * SECTORS_PER_PAGE;
  int i;
  for(i = 0; i < SECTORS_PER_PAGE; i++){
    block_read(swap_device, s + i, kaddr + i * BLOCK_SECTOR_SIZE);
  }
  bitmap_set_multiple (swap_bitmap, used_index, 1, false);
  lock_release(&swap_lock);
}


size_t swap_out(void* kaddr){
  struct block *swap_device;
  lock_acquire(&swap_lock);
  swap_device = block_get_role (BLOCK_SWAP);
  size_t t = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);

  if(t == BITMAP_ERROR){
    return BITMAP_ERROR;
  }
  int i;
  block_sector_t s = t * SECTORS_PER_PAGE;
  for(i = 0 ; i < SECTORS_PER_PAGE; i++){
    block_write(swap_device, s + i, kaddr + i * BLOCK_SECTOR_SIZE);
  }

  lock_release(&swap_lock);
  return t;
}

void swap_set(size_t index){
  lock_acquire(&swap_lock);
  bitmap_set_multiple(swap_bitmap, index, 1, false);
  lock_release(&swap_lock);
}
