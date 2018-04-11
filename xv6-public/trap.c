#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
int mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm);
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }
  if(tf->trapno == T_PGFLT) {
       if(myproc()->pagenum>=MAX_TOTAL_PAGES){
	   kill(myproc()->pid);
	   return ;
        }
        pte_t* pte;
        pte=walkpgdir(myproc()->pgdir,rcr2(),0);
	char* buffer[PGSIZE];
	char* swapinbuf[PGSIZE];
        if(!(*pte)&PTE_PG){
            myproc()->pagenum++;
        }else{
	   //store swap in content in buffer first
	   uint swapinloc=(PTE_ADDR(*pte)>>12)&0xFFFF;
           readFromSwapFile(myproc()->pgdir,swapinbuf,swapinloc,PGSIZE);
	}
        if(myproc()->phy_pagenum<MAX_PSYC_PAGES){
            char * mem;
            mem = kalloc();
            if(mem == 0){
                cprintf("alloc lazy page, out of memory\n");
              return ;
            }
            memset(mem, 0, PGSIZE);
            uint a = PGROUNDDOWN(rcr2());
            mappages(myproc()->pgdir, (char*) a, PGSIZE, V2P(mem), PTE_W | PTE_U);
        }else(){
           //choose a victim to swap out
	    pte* outpage=swapout();
	    writeToSwapFile(myproc()->pgdir,(char *)PTE_ADDR(*outpage),myproc()->swaploc,PGSIZE);
	    *outpage=PTE_FLAG(*outpage)|(myproc()->swaploc<<12);
	    myproc()->swaploc+=PGSIZE;
	    
	   //update flag
	   *outpage&=~PTE_P;
	   *outpage|=PTE_PG;

	   uint a = PGROUNDDOWN(rcr2());
           mappages(myproc()->pgdir, (char*) a, PGSIZE, PTE_ADDR(*outpage), PTE_W | PTE_U);
        }

        //check for swapin
        if(!(*pte)&PTE_PG){
	//no need for swap in, clear contents
            memset((char *)PTE_ADDR(*pte),0,PGSIZE);
        }else{
	//swap in contents
           char * swapindest=(char *)PTE_ADDR(*pte);
	   memcpy(swapindest,swapinbuf,PGSIZE);
        }




  }
  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}

pte_t* swapout(){
	
}
