#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/hrtimer.h>
#include <linux/mm_types.h>
#include <linux/semaphore.h>


static int pid = 0;
static struct task_struct* task;
static int timer_interval_ns;
static int test_case;
static struct hrtimer hr_timer;
static struct hrtimer no_restart_hr_timer;
int present_pages=0;
int swapped_pages=0;
int proc_wss = 0;
static struct semaphore mutex;
static struct semaphore full;
static struct semaphore empty;

module_param(pid, int, 0);



void page_walk(struct vm_area_struct *vma, unsigned long address, struct task_struct *task) {
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *ptep, pte;
    struct page *page;
    int present = 0;
    int accessed = 0;

    pgd = pgd_offset(task->mm, address);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) {
        return;
    }

    p4d = p4d_offset(pgd, address);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) {
        return;
    }

    pud = pud_offset(p4d, address);
    if (pud_none(*pud) || pud_bad(*pud)) {
        return;
    }

    pmd = pmd_offset(pud, address);
    if (pmd_none(*pmd) || pmd_bad(*pmd)) {
        return;
    }

    ptep = pte_offset_map(pmd, address);
    if (!ptep) {
        return;
    }

    pte = *ptep;
    if (pte_present(pte)) {
        present = 1;
        page = pte_page(pte);
	if (pte_young(pte)) {
        accessed = 1;
        pte_mkold(pte);
        set_page_dirty(page);
    }
} else {
    present = 0;
}

pte_unmap(ptep);

if (present) {
    spin_lock(&task->mm->page_table_lock);
    if (accessed) {
        proc_wss++;
    }
    present_pages++;
    spin_unlock(&task->mm->page_table_lock);
} else {
    spin_lock(&task->mm->page_table_lock);
    swapped_pages++;
    spin_unlock(&task->mm->page_table_lock);
}
}

int memory_traversal(struct task_struct *task)
{
struct vm_area_struct *vma;
unsigned long vpage;
for(vma = task->mm->mmap; vma; vma = vma->vm_next)
{
for(vpage = vma->vm_start; vpage < vma->vm_end; vpage += PAGE_SIZE)
{
page_walk(vma, vpage, task);
}
}
return 0;
}


enum hrtimer_restart timer_callback(struct hrtimer *timer) {
struct task_struct *task;
struct vm_area_struct *vma;
unsigned long vpage;
unsigned long present_pages = 0, swapped_pages = 0, proc_wss = 0;
// loop over all processes
for_each_process(task) {
    // skip kernel threads
    if (task->mm == NULL)
        continue;

    // loop over all VMAs
    for (vma = task->mm->mmap; vma; vma = vma->vm_next) {
        // loop over all pages in the VMA
        for (vpage = vma->vm_start; vpage < vma->vm_end; vpage += PAGE_SIZE) {
            page_walk(vma, vpage, task);
        }
    }
}

 
enum hrtimer_restart no_restart_callback(struct hrtimer *timer)
{
down(&mutex);
if (pid == 0) {
task = current;
} else {
task = pid_task(find_vpid(pid), PIDTYPE_PID);
if (task == NULL) {
up(&mutex);
        return HRTIMER_NORESTART;
    }
}
memory_traversal(task);
up(&mutex);
return HRTIMER_NORESTART;
}


void timer_init(void)
{
  if(test_case ==1 || test_case ==2)
    {
      ktime_t ktime = ktime_set(0, timer_interval_ns);  // set timer interval
      hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);  // initialize HRT
      hr_timer.function = &timer_callback;  // set callback function
      hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);  // start timer
    }
  if (test_case == 3)
    {
      ktime_t ktime_no_restart = ktime_set(0, timer_interval_ns);
      hrtimer_init(&no_restart_hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
      no_restart_hr_timer.function = &no_restart_callback;
      hrtimer_start(&no_restart_hr_timer, ktime_no_restart, HRTIMER_MODE_REL);
    }
}

/*
int memory_manager_init (void) 
{
  printk(KERN_INFO "CSE330 Project-2 Kernel Module Inserted\n");
  timer_init();
  return 0;
}

void page_walk(struct vm_area_struct *vma, unsigned long address, struct task_struct *task) {
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *ptep, pte;
    struct page *page;
    int present = 0;
    int accessed = 0;

    pgd = pgd_offset(task->mm, address);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) {
        return;
    }

    p4d = p4d_offset(pgd, address);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) {
        return;
    }

    pud = pud_offset(p4d, address);
    if (pud_none(*pud) || pud_bad(*pud)) {
        return;
    }

    pmd = pmd_offset(pud, address);
    if (pmd_none(*pmd) || pmd_bad(*pmd)) {
        return;
    }

    ptep = pte_offset_map(pmd, address);
    if (!ptep) {
        return;
    }

    pte = *ptep;
    if (pte_present(pte)) {
        present = 1;
        page = pte_page(pte);
	if (pte_young(pte)) {
        accessed = 1;
        pte_mkold(pte);
        set_page_dirty(page);
    }
} else {
    present = 0;
}

pte_unmap(ptep);

if (present) {
    spin_lock(&task->mm->page_table_lock);
    if (accessed) {
        proc_wss++;
    }
    present_pages++;
    spin_unlock(&task->mm->page_table_lock);
} else {
    spin_lock(&task->mm->page_table_lock);
    swapped_pages++;
    spin_unlock(&task->mm->page_table_lock);
}
}

int memory_traversal(struct task_struct *task)
{
struct vm_area_struct *vma;
unsigned long vpage;
for(vma = task->mm->mmap; vma; vma = vma->vm_next)
{
for(vpage = vma->vm_start; vpage < vma->vm_end; vpage += PAGE_SIZE)
{
page_walk(vma, vpage, task);
}
}
return 0;
}
 */
/*
enum hrtimer_restart timer_callback(struct hrtimer *timer) {
struct task_struct *task;
struct vm_area_struct *vma;
unsigned long vpage;
unsigned long present_pages = 0, swapped_pages = 0, proc_wss = 0;
// loop over all processes
for_each_process(task) {
    // skip kernel threads
    if (task->mm == NULL)
        continue;

    // loop over all VMAs
    for (vma = task->mm->mmap; vma; vma = vma->vm_next) {
        // loop over all pages in the VMA
        for (vpage = vma->vm_start; vpage < vma->vm_end; vpage += PAGE_SIZE) {
            page_walk(vma, vpage, task);
        }
    }
}
*/
// print the results
printk(KERN_INFO "Present Pages: %lu, Swapped Pages: %lu, Proc WSS: %lu\n", present_pages, swapped_pages, proc_wss);

if (proc_wss < 3) {
    return HRTIMER_RESTART;
} else {
    return HRTIMER_NORESTART;
}
}
/*
static enum hrtimer_restart no_restart_callback(struct hrtimer *timer)
{
down(&mutex);
if (pid == 0) {
task = current;
} else {
task = pid_task(find_vpid(pid), PIDTYPE_PID);
if (task == NULL) {
up(&mutex);
        return HRTIMER_NORESTART;
    }
}
memory_traversal(task);
up(&mutex);
return HRTIMER_NORESTART;
}
*/
/*
void timer_init(void)
{
  if(test_case ==1 || test_case ==2)
    {
      ktime_t ktime = ktime_set(0, timer_interval_ns);  // set timer interval
      hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);  // initialize HRT
      hr_timer.function = &timer_callback;  // set callback function
      hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);  // start timer
    }
  if (test_case == 3)
    {
      ktime_t ktime_no_restart = ktime_set(0, timer_interval_ns);
      hrtimer_init(&no_restart_hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
      no_restart_hr_timer.function = &no_restart_callback;
      hrtimer_start(&no_restart_hr_timer, ktime_no_restart, HRTIMER_MODE_REL);
    }
}
*/


int memory_manager_init (void) 
{
  printk(KERN_INFO "CSE330 Project-2 Kernel Module Inserted\n");
  timer_init();
  return 0;
}

void memory_manager_exit(void)
{
down(&mutex);
hrtimer_cancel(&hr_timer);
hrtimer_cancel(&no_restart_hr_timer);
up(&mutex);
printk(KERN_INFO "CSE330 Project-2 Kernel Module Removed\n");
}

module_init(memory_manager_init);
module_exit(memory_manager_exit);

MODULE_LICENSE("GPL");
