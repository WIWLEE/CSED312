#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "userprog/signal.h"
#include "userprog/process.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/exception.h"
#include "vm/vm.h"
#include "vm/frame.h"

static void syscall_handler (struct intr_frame *);

static int get_user(const uint8_t *uaddr);
static bool put_user(uint8_t *udst, uint8_t byte);
struct file* get_file_from_fd(int fd);
bool validate_read(void *p, int size);
bool validate_write(void *p, int size);

static void (*syscall_table[20])(struct intr_frame*) = {
  sys_halt,
  sys_exit,
  sys_exec,
  sys_wait,
  sys_create,
  sys_remove,
  sys_open,
  sys_filesize,
  sys_read,
  sys_write,
  sys_seek,
  sys_tell,
  sys_close,
  sys_mmap,
  sys_munmap
}; // syscall jmp table

//해당 포인터의 size만큼이 valid한지를 확인
bool check_valid_buffer(void* buffer, unsigned size, void* esp, bool to_write){
  int i;
  for(i = 0; i < size; i ++){
    //user vaddr 내부인지
    if(!is_user_vaddr(buffer + i) || buffer + i < (void *)0x08048000UL)
      return false;

    //vme가 null일 떄
    struct vm_entry* vme = find_vme(buffer + i);
    if(vme == NULL){
      //확장해야 하는 조건
      //stack의 시작 지점은 PHYS_BASE로 지정 
      //grow할 수 있는 stack의 max limit가 8MB이므로 PHYS_BASE – 8*1024*1024 보다 address가 작다면 이는 늘릴 수 있는 stack의 한도를 넘은 것
      //또한 현재 esp보다 32 byte 이하로 내려가면 안 된다
      //조건이 맞으면 stack 확장
      int32_t addr_compare = (int32_t)(void*)(buffer + i);
      if(is_user_vaddr (addr_compare) && esp - (void*)(addr_compare) <= 32 && 0xC0000000UL - addr_compare <= 8 * 1024 * 1024){
        expand_stack(buffer + i);
        vme = find_vme(buffer+i);
      }
      else{
        return false;
      }
    }

    //write 함수 검증에 사용되는데 Writable하지 않으면 안된다.
    if(to_write && !vme->writable){
      return false;
    }
    //physical memory에 로드 되어있지 않으면 page fault handler 함수 호출
    if(!vme->is_loaded)
      handle_mm_fault(vme);

  }
  return true;

}

/* Reads a byte at user virtual address UADDR.
  UADDR must be below PHYS_BASE.
  Returns the byte value if successful, -1 if a segfault
  occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
        : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
  UDST must be below PHYS_BASE.
  Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
        : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

struct file* get_file_from_fd(int fd) {

  struct list_elem *e;
  struct thread *t = thread_current();
  struct fd_elem *fd_elem;

  for (e = list_begin (&t->fd_table); e != list_end (&t->fd_table);
       e = list_next (e))
  {
    fd_elem = list_entry (e, struct fd_elem, elem);
    if(fd_elem->fd == fd)
      return fd_elem->file_ptr;
  }
  return NULL;
}

bool validate_read(void *p, int size) {
  int i = 0;
  if(p >= PHYS_BASE || p + size >= PHYS_BASE) return false;
  for(i = 0; i < size; i++) {
    if(get_user(p + i) == -1)
      return false;
  }
  return true;
}

bool validate_write(void *p, int size) {
  int i = 0;
  if(p >= PHYS_BASE || p + size >= PHYS_BASE) return false;
  for(i = 0; i < size; i++) {
    if(put_user(p + i, 0) == false)
      return false;
  }
  return true;
}

void kill_process() {
  send_signal(-1, SIG_WAIT);
  printf ("%s: exit(%d)\n", thread_current()->name, -1);
  thread_exit();
}

void
syscall_init (void)
{
  lock_init(&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
  int syscall_num = validate_read(f->esp, 4) ? *(int*)(f->esp) : -1;


  if(syscall_num < 0 || syscall_num >= 20) {
    kill_process();
  }

  (syscall_table[syscall_num])(f);
}

// void halt(void)
void sys_halt (struct intr_frame * f UNUSED) {
  shutdown_power_off();
}

// void exit(int status)
void sys_exit (struct intr_frame * f) {
  int status;

  if(!validate_read(f->esp + 4, 4)) kill_process();

  status = *(int*)(f->esp + 4);

  send_signal(status, SIG_WAIT);
  printf ("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit();
}

// pid_t exec(const char *cmd_line)
void sys_exec (struct intr_frame * f) {
  char *cmd_line;
  pid_t pid;
  char *itr;

  if(!validate_read(f->esp + 4, 4)) kill_process();

  cmd_line = *(char**)(f->esp + 4);
  itr = cmd_line;

  if(!validate_read((void*)cmd_line, 1)) kill_process();

  while(*itr != '\0') {
    itr++;
    if(!validate_read((void*)itr, 1)) kill_process();
  }

  pid = process_execute(cmd_line);
  f->eax = pid == -1 ? pid : get_signal(pid, SIG_EXEC);
}

// int wait (pid_t pid)
void sys_wait (struct intr_frame * f) {
  if(!validate_read(f->esp + 4, 4)) kill_process();

  pid_t pid = *(pid_t*)(f->esp + 4);

  f->eax = process_wait(pid);
}

//bool create (const char *file, unsigned initial_size)
void sys_create (struct intr_frame * f) {
  char *name;
  unsigned initial_size;
  char *itr;

  if(!validate_read(f->esp + 4, 8)) kill_process();

  name = *(char **)(f->esp + 4);
  initial_size = *(unsigned*)(f->esp + 8);
  itr = name;

  if(!validate_read((void*)name, 1)) kill_process();

  while(*itr != '\0') {
    if(!validate_read((void*)itr, 1)) kill_process();
      itr++;
  }

  lock_acquire(&file_lock);
  f->eax = filesys_create(name, initial_size);
  lock_release(&file_lock);
}

//bool remove (const char *file)
void sys_remove (struct intr_frame * f) {
  char *name;
  char *itr;

  if(!validate_read(f->esp + 4, 4)) kill_process();

  name = *(char **)(f->esp + 4);
  itr = name;

  if(!validate_read((void*)name, 1)) kill_process();

  while(*itr != '\0') {
    if(!validate_read((void*)itr, 1)) kill_process();
      itr++;
  }

  lock_acquire(&file_lock);
  f->eax = filesys_remove(name);
  lock_release(&file_lock);
}

//int open (const char *file)
void sys_open (struct intr_frame * f) {
  char *name;
  char *itr;
  struct thread *t;
  struct file *file;
  struct list_elem *e;
  struct fd_elem *f_elem;
  struct fd_elem *fd_elem;

  if(!validate_read(f->esp + 4, 4)) kill_process();

  name = *(char **)(f->esp + 4);
  itr = name;

  if(!validate_read((void*)name, 1)) kill_process();

  while(*itr != '\0') {
    if(!validate_read((void*)itr, 1)) kill_process();
    itr++;
  }

  if(itr == name) {
    f->eax = -1;
    return;
  }

  t = thread_current();
  file = filesys_open(name); //if fails, it returns NULL
  f_elem = malloc(sizeof(struct fd_elem));

  if(file == NULL) {
    f->eax = -1;
    return;
  }

  f_elem->fd = 2;
  f_elem->file_ptr = file;

  for (e = list_begin (&t->fd_table); e != list_end (&t->fd_table);
       e = list_next (e))
  {
    fd_elem = list_entry (e, struct fd_elem, elem);
    if(fd_elem->fd > f_elem->fd) {
      e = list_prev(e);
      list_insert(e, &f_elem->elem);
      f->eax = f_elem->fd;
      return;
    }
    f_elem->fd++;
  }
  list_push_back(&t->fd_table, &f_elem->elem);
  f->eax = f_elem->fd;
}

//int filesize (int fd)
void sys_filesize (struct intr_frame * f) {
  int fd;
  struct file *file;

  if(!validate_read(f->esp + 4, 4)) kill_process();

  fd = *(int*)(f->esp + 4);
  file = get_file_from_fd(fd);

  if(file == NULL) f->eax = -1;

  f->eax = file_length(file);
}

//int read (int fd, void *buffer, unsigned size)
void sys_read (struct intr_frame * f) {
  char c;
  unsigned count = 0;
  int fd;
  uint8_t* buffer;
  unsigned size;
  struct file *file;

  if(!validate_read(f->esp + 4, 12)) kill_process();

  fd = *(int*)(f->esp + 4);
  buffer = *(uint8_t**)(f->esp + 8);
  size = *(unsigned*)(f->esp + 12);

  //valid buffer확인
  if(!check_valid_buffer(buffer, size, f->esp, true)) kill_process();

  file = get_file_from_fd(fd);

  if(!validate_write(buffer, size)) kill_process();
  if(fd == 0) {
    c = input_getc();
    while(c != '\n' && c != -1 && count < size) {
      if(!put_user(buffer, c)) kill_process();
      buffer++;
      count++;
      c = input_getc();
    }
    f->eax = count;
  }
  else if(fd == 1) {
    f->eax = -1;
  }
  else {
    if(file == NULL) {
      f->eax = -1;
      return;
    }

    lock_acquire(&file_lock);
    f->eax = file_read(file, buffer, size);
    lock_release(&file_lock);
  }
}

//int write (int fd, const void *buffer, unsigned size)
void sys_write (struct intr_frame * f) {
  int fd;
  char* buffer;
  unsigned size;
  struct file *file;

  if(!validate_read(f->esp + 4, 12)) kill_process();

  fd = *(int*)(f->esp + 4);
  buffer = *(char**)(f->esp + 8);
  size = *(unsigned*)(f->esp + 12);
  file = get_file_from_fd(fd);

  //valid buffer 확인
  if(!check_valid_buffer(buffer, size, f->esp, false)) kill_process();

  if(!validate_read(buffer, size)) kill_process();

  if(fd == 0) {
    f->eax = 0;
  }
  else if(fd == 1) {
    putbuf(buffer, size);
    f->eax = size;
  }
  else {
    if(file == NULL) {
      f->eax = 0;
      return;
    }
    lock_acquire(&file_lock);
    f->eax = file_write (file, buffer, size);
    lock_release(&file_lock);
  }
}

//void seek (int fd, unsigned position)
void sys_seek (struct intr_frame * f) {
  int fd;
  off_t position;
  struct file *file;

  if(!validate_read(f->esp + 4, 8)) kill_process();

  fd = *(int*)(f->esp + 4);
  position = *(int*)(f->esp + 8);
  file = get_file_from_fd(fd);

  if(file == NULL) f->eax = -1;

  lock_acquire(&file_lock);
  file_seek(file, position);
  lock_release(&file_lock);
}

//unsigned tell (int fd)
void sys_tell (struct intr_frame * f) {
  if(!validate_read(f->esp + 4, 4)) kill_process();
  int fd = *(int*)(f->esp + 4);
  struct file *file = get_file_from_fd(fd);
  if(file == NULL)
    f->eax = -1;
  lock_acquire(&file_lock);
  f->eax = file_tell(file);
  lock_release(&file_lock);
}

//void close (int fd)
void sys_close (struct intr_frame * f) {
  int fd;
  struct file *file;
  struct thread *t;
  struct list_elem *e;
  struct fd_elem *fd_elem;

  if(!validate_read(f->esp + 4, 4)) kill_process();

  fd = *(int*)(f->esp + 4);
  file = get_file_from_fd(fd);
  t = thread_current();

  lock_acquire(&file_lock);
  file_close(file);
  lock_release(&file_lock);

  for (e = list_begin (&t->fd_table); e != list_end (&t->fd_table);
       e = list_next (e))
  {
    fd_elem = list_entry (e, struct fd_elem, elem);
    if(fd_elem->fd == fd) {
      list_remove(e);
      free(fd_elem);
      return;
    }
  }
}

void sys_mmap (struct intr_frame * f){
  int fd;
  void *addr;
  if(!validate_read(f->esp + 4, 8)) kill_process();
  fd = *(int*)(f->esp + 4);
  addr = *(void**)(f->esp + 8);
  // 매개변수로 들어오는 intr_frame에서 fd와 addr을 구하여 값의 유효성을 확인한다.

  if (pg_ofs (addr) != 0 || !addr || is_user_vaddr (addr) == false){
    f->eax = -1;
    return;
  } // 유효하지 않으면 리턴


  struct file* file = get_file_from_fd(fd); // 원하는 파일을 찾는다.
  if(file == NULL){
    f->eax = -1;
    return;
  }

  //mmap_file* m에 malloc을 통해서 할당하고, 파일을 m에 집어넣는다.
  struct mmap_file* m = malloc(sizeof (struct mmap_file));

  m->mapid = thread_current()->current_map_id++; // 매핑을 ID로 관리한다. 
  m->file = file_reopen(file);
  list_init(&m->vme_list); //m의 vme_list를 초기화한다.
  if(m->file == NULL){
    free(m);
    f->eax = -1;
    return;
  }

  list_push_back(&thread_current()->mmap_list, &m->elem);
  off_t length = file_length(m->file);
  size_t offset = 0;

  while(length > 0){
    if(find_vme(addr+offset)){
      f->eax = -1;
      return;
    }

    //vme를 생성하고, 초기화 시킨다.
    struct vm_entry * vme = vm_entry_alloc( addr + offset);
    if (vme == NULL){
      f->eax = -1;
      return;
    }
    vme->writable = true;
    vme->file = m->file;
    vme->file_offset = offset;
    vme->file_bytes = length >= PGSIZE ? PGSIZE : length;
    vme->type = VM_FILE;

    offset += vme->file_bytes;
    length -= vme->file_bytes;
    insert_vme(thread_current()->vm, vme);
    list_push_back(&m->vme_list, &vme->mmap_elem); // vme list에 넣어서 매핑한다.
  }
  f->eax = m->mapid;

}

void sys_munmap(struct intr_frame * f){
  if(!validate_read(f->esp + 4, 4)) kill_process();
  int id = *(int*)(f->esp + 4);
  struct mmap_file* m = NULL;
  struct list map_list = thread_current()->mmap_list; // 현재 thread의 mmap_list
  struct list_elem *e;

  //unmap할 파일을 찾기
  for(e = list_begin(&map_list); e != list_end(&map_list); e = list_next(e)){
    struct mmap_file* tmp = list_entry(e, struct mmap_file, elem);
    if(tmp->mapid == id){
      m = tmp;
      break;
    }
  }
  if(m == NULL)
    return;
  do_munmmap(m); // 실제 munmap 실행
}



void do_munmmap(struct mmap_file* m){
  struct list_elem *e;

  //mmap_file* m의 vme_list를 탐색
  for(e = list_begin(&m->vme_list); e != list_end(&m->vme_list); ){
    struct vm_entry* vme = list_entry(e ,struct vm_entry, mmap_elem);
    lock_acquire(&file_lock);

    //dirty 하니까 write 해주고
    if(vme->is_loaded && pagedir_is_dirty(thread_current()->pagedir, vme->vaddr)){
      lock_release(&file_lock);
      file_write_at(vme->file, vme-> vaddr, vme->file_bytes, vme->file_offset);
    }
    else
      lock_release(&file_lock);

    //remove 해준다.
    vme->is_loaded = false;
    e = list_remove(e);
    delete_vme(thread_current()->vm, vme);
  }
  list_remove(&m->elem);
  free(m);
  return;
}
