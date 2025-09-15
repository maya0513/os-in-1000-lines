#include "kernel.h"

// 外部シンボル宣言
extern char __kernel_base[];
extern char __free_ram_end[];

// グローバル変数
struct process procs[PROCS_MAX];
struct process *proc_a;
struct process *proc_b;
struct process *current_proc;
struct process *idle_proc;

// コンテキストスイッチ
__attribute__((naked)) void switch_context(uint32_t *prev_sp, uint32_t *next_sp)
{
  __asm__ __volatile__(
      // 実行中プロセスのスタックへレジスタを保存
      "addi sp, sp, -13 * 4\n"
      "sw ra,  0  * 4(sp)\n"
      "sw s0,  1  * 4(sp)\n"
      "sw s1,  2  * 4(sp)\n"
      "sw s2,  3  * 4(sp)\n"
      "sw s3,  4  * 4(sp)\n"
      "sw s4,  5  * 4(sp)\n"
      "sw s5,  6  * 4(sp)\n"
      "sw s6,  7  * 4(sp)\n"
      "sw s7,  8  * 4(sp)\n"
      "sw s8,  9  * 4(sp)\n"
      "sw s9,  10 * 4(sp)\n"
      "sw s10, 11 * 4(sp)\n"
      "sw s11, 12 * 4(sp)\n"

      // スタックポインタの切り替え
      "sw sp, (a0)\n"
      "lw sp, (a1)\n"

      // 次のプロセスのスタックからレジスタを復元
      "lw ra,  0  * 4(sp)\n"
      "lw s0,  1  * 4(sp)\n"
      "lw s1,  2  * 4(sp)\n"
      "lw s2,  3  * 4(sp)\n"
      "lw s3,  4  * 4(sp)\n"
      "lw s4,  5  * 4(sp)\n"
      "lw s5,  6  * 4(sp)\n"
      "lw s6,  7  * 4(sp)\n"
      "lw s7,  8  * 4(sp)\n"
      "lw s8,  9  * 4(sp)\n"
      "lw s9,  10 * 4(sp)\n"
      "lw s10, 11 * 4(sp)\n"
      "lw s11, 12 * 4(sp)\n"
      "addi sp, sp, 13 * 4\n"
      "ret\n");
}

__attribute__((naked)) void user_entry(void)
{
  __asm__ __volatile__(
      "csrw sepc, %[sepc]\n"
      "csrw sstatus, %[sstatus]\n"
      "sret\n"
      :
      : [sepc] "r"(USER_BASE),
        [sstatus] "r"(SSTATUS_SPIE | SSTATUS_SUM));
}

struct process *create_process(const void *image, size_t image_size)
{
  // 空いているプロセス管理構造体を探す
  struct process *proc = NULL;
  int i;
  for (i = 0; i < PROCS_MAX; i++)
  {
    if (procs[i].state == PROC_UNUSED)
    {
      proc = &procs[i];
      break;
    }
  }

  if (!proc)
    PANIC("no free process slots");

  // switch_context() で復帰できるように、スタックに呼び出し先保存レジスタを積む
  uint32_t *sp = (uint32_t *)&proc->stack[sizeof(proc->stack)];
  *--sp = 0;                    // s11
  *--sp = 0;                    // s10
  *--sp = 0;                    // s9
  *--sp = 0;                    // s8
  *--sp = 0;                    // s7
  *--sp = 0;                    // s6
  *--sp = 0;                    // s5
  *--sp = 0;                    // s4
  *--sp = 0;                    // s3
  *--sp = 0;                    // s2
  *--sp = 0;                    // s1
  *--sp = 0;                    // s0
  *--sp = (uint32_t)user_entry; // ra

  uint32_t *page_table = (uint32_t *)alloc_pages(1);

  // カーネルのページをマッピングする
  for (paddr_t paddr = (paddr_t)__kernel_base; paddr < (paddr_t)__free_ram_end; paddr += PAGE_SIZE)
    map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);

  map_page(page_table, VIRTIO_BLK_PADDR, VIRTIO_BLK_PADDR, PAGE_R | PAGE_W);

  // ユーザーのページをマッピングする
  for (uint32_t off = 0; off < image_size; off += PAGE_SIZE)
  {
    paddr_t page = alloc_pages(1);

    // コピーするデータがページサイズより小さい場合を考慮
    size_t remaining = image_size - off;
    size_t copy_size = PAGE_SIZE <= remaining ? PAGE_SIZE : remaining;

    // 確保したページにデータをコピー
    memcpy((void *)page, image + off, copy_size);

    // ページテーブルにマッピング
    map_page(page_table, USER_BASE + off, page, PAGE_U | PAGE_R | PAGE_W | PAGE_X);
  }

  // 各フィールドを初期化
  proc->pid = i + 1;
  proc->state = PROC_RUNNABLE;
  proc->sp = (uint32_t)sp;
  proc->page_table = page_table;
  return proc;
}

void yield(void)
{
  // 実行可能なプロセスを探す
  struct process *next = idle_proc;
  for (int i = 0; i < PROCS_MAX; i++)
  {
    struct process *proc = &procs[(current_proc->pid + i) % PROCS_MAX];
    if (proc->state == PROC_RUNNABLE && proc->pid > 0)
    {
      next = proc;
      break;
    }
  }

  // 現在実行中のプロセス以外に、実行可能なプロセスがない。戻って処理を続行する
  if (next == current_proc)
    return;

  __asm__ __volatile__(
      "sfence.vma\n"
      "csrw satp, %[satp]\n"
      "sfence.vma\n"
      "csrw sscratch, %[sscratch]\n"
      :
      : [satp] "r"(SATP_SV32 | ((uint32_t)next->page_table / PAGE_SIZE)),
        [sscratch] "r"((uint32_t)&next->stack[sizeof(next->stack)]));

  // コンテキストスイッチ
  struct process *prev = current_proc;
  current_proc = next;
  switch_context(&prev->sp, &next->sp);
}