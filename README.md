# Wat?
This project came out of the question: What's the fastest way to communicate between processes, and just how "fast" is it?

For the impatient, you need the following build dependencies on Ubuntu 16.04+. Earlier versions of Ubuntu don't have all the requisite dependencies, so you'll have trouble building with it. You must have an x86_64 processor with the constant_tsc feature. You can run `cat /proc/cpuinfo |grep constant_tsc` in order to determine whether or not your processor supports it. A processor that supports it will output some number of lines, one that doesn't will be blank.

#### Dependencies:

* libck-dev
* build-essential
* cmake

It's recommended you install the following tools as well:

* cpufrequtils
* schedtool
* linux-tools-common
* linux-tools-generic
* linux-tools-`uname -r`

Or, in a few quick commands:

```
# apt-get install -y build-essential libck-dev cmake 
## Recommended:
# apt-get install -y strace schedtool cpufrequtils linux-tools-common linux-tools-generic linux-tools-`uname -r`

# Debug symbols, based on: https://wiki.ubuntu.com/Debug%20Symbol%20Packages
# echo "deb http://ddebs.ubuntu.com $(lsb_release -cs) main restricted universe multiverse
deb http://ddebs.ubuntu.com $(lsb_release -cs)-updates main restricted universe multiverse
deb http://ddebs.ubuntu.com $(lsb_release -cs)-proposed main restricted universe multiverse" | \
sudo tee -a /etc/apt/sources.list.d/ddebs.list
# sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 428D7C01 C8CAB6595FDFF622
# sudo apt-get update
# apt-get install -y libc6-dbg
```

In order to build it:

```
# mkdir build/
# cmake ..
# make
```

## What's included?
### Message passing benchmarks
These benchmarks test how long it takes to pass back and forth a 1 byte number between processes in as low latency as possible. Please read-on on how to run them. 

These are message passing benchmark. It comes in a few different flavours:

* `bench`: Shared memory based message passing
* `bench2`: Shared-memory, single core based message passing with serialized execution
* `bench3`: POSIX Queue based message passing benchmark
* `bench4`: Unix Socket based message passing benchmark

All of these **EXCEPT** `bench2` require invoking two separate processes (from the same directory). They require a sender mode process, and a receiver mode process:

Sender:
`./bench s`

Receiver:
`./bench r`

### How slow is my computer benchmarks?
#### cgt
CGT determines how long it takes for your computer to call `clock_gettime(CLOCK_MONOTONIC, *)`. The output is in cycles. 
 
#### gp
GP determines how long a syscall takes on your computer, as an indirect mechanism to measure the cost of a context-switch. It uses the `getpid` syscall, and therefore it's very cheap. The output is in cycles.

### rdtscp
RDTSCP measures both how long it takes for your system to execute the RDTSCP instruction, and how to convert between nanoseconds, and cycles. The output looks something like this:

```
# schedtool -F -p 99 -e taskset -c 0 ./rdtscp 
Nanoseconds per rdtscp: 9
Total Nanoseconds: 48489389
Total Counter increases: 169902778
Nanoseconds per cycle: 0.285395
Cycles per nanosecond: 3.503917
```

In order to convert from cycles to nanoseconds, you multiply times the constant `0.285395`. Also, "Nanoseconds per rdtscp" determines roughly how many ns+ your benchmark will be. 

### sy
SY measured how long your kernel / computer / processor takes to call `sched_yield()`. Sched_yield is a syscall which context switches to the kernel and in turn calls `schedule()`, which hands off the current execution context to the system's pool of tasks. It's useful to determine how long involuntary scheduler switches would cost you, and running it under perf can help you find out where your computer is being slow.

## Recommendations on running tests
### CPU Frequency Utilities
You would be surprised how much modern processor power savings features hurt performance. In my testing I found a 220% latency decrease. For this reason, I recommend turning on the performance governor like so:

```
# PROCESSORS=$(nproc)
# cpufreq-set -c 0-$(($PROCESSORS-1)) -g performance
```
### Core Pinning
Because we're testing how long it takes to pass messages, it's valuable to pin your tasks to cores. For this, you can use the `taskset` command like so:

```
# taskset -c 2 ./bench2
```

I also recommend denying the scheduler the ability to scheduler processes on the other thread that belongs to the given hyperthreaded core you've scheduled your process(es) upon. Usually, while testing, I run benchmarks on cores 0, and 2, and I deny schedulong on 1 and 3 because they're thread siblings of those cores. 

### Scheduler Fun
**WARNING** **WARNING** **WARNING** **WARNING** 

**Do not even think of doing this if you have fewer than *3 physical* cores**

Be very careful with this. If you use `SCHED_FIFO` you are completely taking over a CPU core, and stealing it away from the operating system for everything other than hardware interrupts. 

In order to do this, you prefix the command with: `schedtool -F -p 99 -e`. `-F` tells schedtool to use `SCHED_FIFO`, which tells the Linux kernel to never preempt your task. `-p 99` tells the kernel that it's priority 99. `-e` indicates to run the command following.

Example:

```
### A run with SCHED_FIFO:
# schedtool -F -p 99 -e ./bench2
Average cycles: 92
Median Iteration Cycles: 90
Min Cycles: 70
95th Percentile Cycles: 110
Invol Ctx Switches: 2
Voluntary Ctx Switches: 0

### A run without SCHED_FIFO:
# ./bench2
Average cycles: 93
Median Iteration Cycles: 90
Min Cycles: 72
95th Percentile Cycles: 112
Invol Ctx Switches: 82
Voluntary Ctx Switches: 0
```

### All Together
Preparation:

```
# PROCESSORS=$(nproc)
# cpufreq-set -c 0-$(($PROCESSORS-1)) -g performance
# echo 0 > /sys/devices/system/cpu/cpu1/online
# echo 0 > /sys/devices/system/cpu/cpu3/online
```
Process Sender (Start first):

```
# schedtool -F -p 99 -e taskset -c 0 ./bench s 
```

Process Receiver (Start second):

```
# schedtool -F -p 99 -e taskset -c 2 ./bench r 
```

Cleanup:

```
# echo 1 > /sys/devices/system/cpu/cpu1/online
# echo 1 > /sys/devices/system/cpu/cpu3/online
```

# Samples:
All tests done on [packet.net](https://www.packet.net/bare-metal/)'s Type 1 server, with `performance` governor as `root`. 

Example CPU:

```
processor	: 0
vendor_id	: GenuineIntel
cpu family	: 6
model		: 94
model name	: Intel(R) Xeon(R) CPU E3-1240 v5 @ 3.50GHz
stepping	: 3
microcode	: 0x6a
cpu MHz		: 3899.628
cache size	: 8192 KB
physical id	: 0
siblings	: 8
core id		: 0
cpu cores	: 4
apicid		: 0
initial apicid	: 0
fpu		: yes
fpu_exception	: yes
cpuid level	: 22
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe syscall nx pdpe1gb rdtscp lm constant_tsc art arch_perfmon pebs bts rep_good nopl xtopology nonstop_tsc aperfmperf eagerfpu pni pclmulqdq dtes64 monitor ds_cpl vmx smx est tm2 ssse3 sdbg fma cx16 xtpr pdcm pcid sse4_1 sse4_2 x2apic movbe popcnt tsc_deadline_timer aes xsave avx f16c rdrand lahf_lm abm 3dnowprefetch epb intel_pt tpr_shadow vnmi flexpriority ept vpid fsgsbase tsc_adjust bmi1 hle avx2 smep bmi2 erms invpcid rtm mpx rdseed adx smap clflushopt xsaveopt xsavec xgetbv1 dtherm ida arat pln pts hwp hwp_notify hwp_act_window hwp_epp
bugs		:
bogomips	: 7008.62
clflush size	: 64
cache_alignment	: 64
address sizes	: 39 bits physical, 48 bits virtual
```

## Bench: Shared Memory

```
# schedtool -F -p 99 -e taskset -c 0 ./bench s
Median Iteration Time: 492
Min Time: 300
95th percentile Time: 600
Invol Ctx Switches: 0
Voluntary Ctx Switches: 0
```
## Bench2: Sequential Execution

```
# schedtool -F -p 99 -e taskset -c 0 ./bench2 
Average cycles: 73
Median Iteration Cycles: 74
Min Cycles: 68
95th Percentile Cycles: 76
Invol Ctx Switches: 0
Voluntary Ctx Switches: 0
```
## Bench3: SysV / POSIX Queues

```
# schedtool -F -p 99 -e taskset -c 0 ./bench3 s 
Median Iteration Time: 10454
Min Time: 5612
95th Percentile Time: 10850
Invol Ctx Switches: 1
Voluntary Ctx Switches: 3999695
```

## Bench4: UNIX Domain Sockets
```
# schedtool -F -p 99 -e taskset -c 0 ./bench4 s 
Median Iteration Time: 11250
Min Time: 7326
95th percentile Time: 11698
Invol Ctx Switches: 0
Voluntary Ctx Switches: 4001850
```

## CGT: How long do VDSO syscalls take?
```
# schedtool -F -p 99 -e taskset -c 2 ./cgt
Average cycles per clock_gettime: 57
Minimum cycles per iteration: 84
Median cycles per iteration: 92
95th percentile cycles per iteration: 94
```
## GP: How long do normal syscalls take:
```
# ./gp
Average cycles per getpid: 129
Minimum cycles per iteration: 156
Median cycles per iteration: 162
95th percentile cycles per iteration: 164
```
## RDTSCP: How long does is a nanosecond (or a cycle)?
```
Nanoseconds per rdtscp: 8
Total Nanoseconds: 424982501
Total Counter increases: 1489109408
Nanoseconds per cycle: 0.285394
Cycles per nanosecond: 3.503931
```
