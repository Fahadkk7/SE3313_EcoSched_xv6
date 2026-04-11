// Saved registers for kernel context switches.
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// Per-CPU state.
struct cpu {
  struct proc *proc;          // The process running on this cpu, or null.
  struct context context;     // swtch() here to enter scheduler().
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

// per-process data for the trap handling code in trampoline.S.
// sits in a page by itself just under the trampoline page in the
// user page table. not specially mapped in the kernel page table.
// uservec in trampoline.S saves user registers in the trapframe,
// then initializes registers from the trapframe's
// kernel_sp, kernel_hartid, kernel_satp, and jumps to kernel_trap.
// usertrapret() and userret in trampoline.S set up
// the trapframe's kernel_*, restore user registers from the
// trapframe, switch to the user page table, and enter user space.
// the trapframe includes callee-saved user registers like s0-s11 because the
// return-to-user path via usertrapret() doesn't return through
// the entire kernel call stack.
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // kernel page table
  /*   8 */ uint64 kernel_sp;     // top of process's kernel stack
  /*  16 */ uint64 kernel_trap;   // usertrap()
  /*  24 */ uint64 epc;           // saved user program counter
  /*  32 */ uint64 kernel_hartid; // saved kernel tp
  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // wait_lock must be held when using this:
  struct proc *parent;         // Parent process

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)

  // Feature 3: Eco-Aware Scheduler fields
  uint cpu_ticks;              // Total timer ticks consumed by this process
  uint eco_skip;               // Scheduler skip counter for throttling

  // Feature 4: Eco-Credit System
  int eco_credits;             // Per-process eco-credit score (0..ECO_MAX_CREDITS)
  uint credit_window_start;    // Global tick count when current credit window began
  uint credit_cpu_ticks;       // CPU ticks consumed within the current credit window
};

// Sensor types
#define SENSOR_TEMP  0
#define SENSOR_POWER 1

// Eco state constants (returned by getecostate syscall)
#define ECO_NORMAL   0
#define ECO_ECO      1
#define ECO_CRITICAL 2

// Threshold values for the sustainability monitor
#define THRESH_CRITICAL_TEMP  85
#define THRESH_CRITICAL_POWER 75
#define THRESH_ECO_TEMP       70
#define THRESH_ECO_POWER      50

struct sensor_data {
  int temperature;
  int power_usage;
};

// Feature 3: Eco-Aware Scheduler
// Processes with cpu_ticks above this are considered "heavy" and throttled.
#define ECO_CPU_TICK_THRESHOLD  10
// In ECO mode, heavy processes run 1 out of every ECO_SKIP_MOD scheduler rounds.
#define ECO_SKIP_MOD            2
// In CRITICAL mode, heavy processes run 1 out of every CRIT_SKIP_MOD rounds.
#define CRIT_SKIP_MOD           4

// Feature 4: Eco-Credit System
#define ECO_INITIAL_CREDITS  5   // Starting credits for every new process
#define ECO_MAX_CREDITS      10  // Upper bound for eco_credits
#define ECO_MIN_CREDITS      0   // Lower bound for eco_credits
#define ECO_CREDIT_WINDOW    15  // Ticks per credit evaluation window
#define ECO_HEAVY_WINDOW     10  // If window_ticks >= this, lose 1 credit
#define ECO_LIGHT_WINDOW     3   // If window_ticks <= this, gain 1 credit
