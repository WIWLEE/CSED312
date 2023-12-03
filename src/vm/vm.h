#ifndef VM_VM_H
#define VM_VM_H

#include "filesys/off_t.h"
#include <hash.h>
#include <list.h>

// Virtual Memory Page 3 Type
#define VM_BIN 0 // 바이너리 파일로부터 데이터를 로드
#define VM_FILE 1 // 파일로부터 데이터를 로드
#define VM_ANON 2 // 스왑 영역으로부터 데이터를 로드

// vm_entry -> Supplement page struct
struct vm_entry{
  bool writable; 
  int type;
  void *vaddr; // virtual address

  struct thread* thread; // owner thread
  struct file* file; // 매핑된 파일과 offset, bytes
  off_t file_offset;
  off_t file_bytes;

  size_t used_index; // 스왑 슬롯 인덱스

  struct hash_elem elem;
  struct list_elem mmap_elem;
  bool is_loaded; // physical memory에 탑재되어 있는지
};

// file mapping을 위한 structure
struct mmap_file{
  int mapid; // mapping id
  struct file* file; // 매핑 파일
  struct list_elem elem; // mmap_file list -> 헤드는 struct thread
  struct list vme_list; // mmap_file에 해당하는 vm_entry list
};


void vm_destory(struct hash *vm);
void vm_init(struct hash* vm);
struct vm_entry* find_vme(void *addr);
bool insert_vme(struct hash* vm, struct vm_entry *vme);
bool delete_vme(struct hash* vm, struct vm_entry *vme);
struct vm_entry* vm_entry_alloc(void* addr);

static unsigned vm_hash_func(const struct hash_elem *e, void* aux);
static bool vm_less_func(const struct hash_elem *e1, const struct hash_elem *e2, void* aux);
static void vm_destory_func(struct hash_elem* e, void *aux);

#endif /* vm/vm.h */
