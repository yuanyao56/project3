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

pte_t* swapout(void);

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
	uint va=rcr2();
       if(myproc()->pagenum>=MAX_TOTAL_PAGES){
	   kill(myproc()->pid);
	   return ;
        }
        pte_t* pte;
        pte=walkpgdir(myproc()->pgdir,(void *)va,0);
	char swapinbuf[PGSIZE];
        if(!((*pte)&PTE_PG)){
            myproc()->pagenum++;
        }else{
	   //store swap in content in buffer first
	   uint swapinloc=(PTE_ADDR(*pte)>>12)&0xFFFF;
           readFromSwapFile(myproc(),swapinbuf,swapinloc,PGSIZE);
	}
        if(myproc()->phy_pagenum<MAX_PSYC_PAGES){
            char * mem;
            mem = kalloc();
            if(mem == 0){
                cprintf("alloc lazy page, out of memory\n");
              return ;
            }
            memset(mem, 0, PGSIZE);
            uint a = PGROUNDDOWN(va);
            mappages(myproc()->pgdir, (char*) a, PGSIZE, V2P(mem), PTE_W | PTE_U);
        }else{
           //choose a victim to swap out
	    pte_t* outpage=swapout();
	    writeToSwapFile(myproc(),(char *)PTE_ADDR(*outpage),myproc()->swaploc,PGSIZE);
	    *outpage=PTE_FLAGS(*outpage)|(myproc()->swaploc<<12);
	    myproc()->swaploc+=PGSIZE;
	    
	   //update flag
	   *outpage&=~PTE_P;
	   *outpage|=PTE_PG;

	   uint a = PGROUNDDOWN(va);
           mappages(myproc()->pgdir, (char*) a, PGSIZE, PTE_ADDR(*outpage), PTE_W | PTE_U);
        }

        //check for swapin
        if(!((*pte)&PTE_PG)){
	//no need for swap in, clear contents
            memset((char *)PTE_ADDR(*pte),0,PGSIZE);
        }else{
	//swap in contents
           char * swapindest=(char *)PTE_ADDR(*pte);
	   memmove(swapindest,swapinbuf,PGSIZE);
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
     tf->trapno == T_IRQ0+IRQ_TIMER){
	#ifdef LRU
	int i;
	pte_t* pte1;
	for(i=0;i<p->sz;i+=PGSIZE){
		pte1 = walkpgdir(myproc()->pgdir, (char *)i, 0);
		if(*pte1 & PTE_A){
			int j = 0;
			for(j=0; j<15; j++){
				if(PTE_ADDR(*myproc()->victims[j].pte)==PTE_ADDR(*pte1) && myproc()->victims[j].inStack==1){
					if (myproc()->head! = j && myproc()->tail != j){
						myproc()->victims[myproc()->victims[j].previous].next = myproc()->victims[j].next;
                                        	myproc()->victims[myproc()->victims[j].next].previous = myproc()->victims[j].previous;
                                        	myproc()->victims[j].previous = -1;
                                        	myproc()->victims[j].next = myproc()->head;
                                        	myproc()->victims[myproc()->head].previous = j;
                                        	myproc()->head = j;
					}
					if (myproc()->head != j && myproc()->tail == j){
                                        	myproc()->tail = myproc()->victims[j].previous;
                                        	myproc()->victims[myproc()->tail].next = -1;
                                        	myproc()->victims[j].next = myproc()->head;
                                        	myproc()->victims[j].previous = -1;
                                        	myproc()->victims[myproc()->head].previous = j;
                                        	myproc()->head = j;
                                	}
					break;
				}
			}
		}
		*pte1 &= ~PTE_A;
	}
	#endif
    	yield();
  }
  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
unsigned long randstate = 1;
unsigned int
rand()
{
  randstate = randstate * 1664525 + 1013904223;
  return randstate;
}


pte_t* swapout(){
	//#ifdef RAND
	struct proc* p = myproc();
        pte_t *pte;
        pte_t *ptes[15];
        int i,j=0,total=0;
        for(i=0; i<p->sz; i+=PGSIZE){
        	pte = walkpgdir(p->pgdir, (char *)i, 0);
        	if(*pte & PTE_P){ //check PTE_P
            		total++;
            		ptes[j++]=pte;
        	}
       }   
       return ptes[rand()%total];
	//#endif	
	
	#ifdef LRU
        pte_t* ret = myproc()->victims[myproc()->tail].pte;
	myproc()->victims[myproc()->tail].inStack = 0;
	if(myproc()->victims[myproc()->tail].previous < 0)
		return ret;
	myproc()->victims[myproc()->victims[myproc()->tail].previous].next = -1;
	myproc()->tail = myproc()->victims[myproc()->tail].previous;
	return ret;
        #endif
	
	
	#ifdef FIFO
	pte_t* ret = myproc()->victims[myproc()->head].pte;
	myproc()->victims[myproc()->head].inStack = 0;
	if(myproc()->victims[myproc()->head].next < 0)
		return ret;
	myproc()->victims[myproc()->victims[myproc()->head].next].previous = -1;
	myproc()->head = myproc()->victims[myproc()->head].next;
	return ret;
	#endif
	
	
}

int allocateStack(pte_t* pte1){
	#ifdef LRU
	int i;
	int b=0;
	for(i=0; i<15; i++){
		if(myproc()->victims[i].inStack!=1)
			b++;
	}
	if (b==0){
		return -1;
	}
	else if(b==15){
		myproc()->head = 0;
		myproc()->tail = 0;
		myproc()->victims[0].previous = -1;
		myproc()->victims[0].next = -1;
		myproc()->victims[0].inStack = 1;
		myproc()->victims[0].pte = pte1;
		return 1;
	}
	else{
		for(i=0; i<15; i++){
			if(myproc()->victims[i].inStack!=1){
				myproc()->victims[i].inStack = 1;
				myproc()->victims[i].pte = pte1;
				myproc()->victims[i].previous = -1;
				myproc()->victims[i].next = myproc()->head;
				myproc()->victims[myproc()->head].previous = i;
				myproc()->head = i;
				return 2;
			}
		}
	}
	return -2;
	#endif
	
	
	#ifdef FIFO
	int i;
	int b=0;
	for(i=0; i<15; i++){
		if(myproc()->victims[i].inStack!=1)
			b++;
	}
	
	if (b==0){
		return -1;
	}
	
	else if(b==15){
		myproc()->head = 0;
		myproc()->tail = 0;
		myproc()->victims[0].previous = -1;
		myproc()->victims[0].next = -1;
		myproc()->victims[0].inStack = 1;
		myproc()->victims[0].pte = pte1;
		return 1;
	}
	else{
		for(i=0; i<15; i++){
			if(myproc()->victims[i].inStack!=1){
				myproc()->victims[i].inStack = 1;
				myproc()->victims[i].pte = pte1;
				myproc()->victims[i].next = -1;
				myproc()->victims[i].previous = myproc()->tail;
				myproc()->victims[myproc()->tail].next = i;
				myproc()->tail = i;
				return 2;
			}
		}
	}	
	return -2;
	#endif
}
