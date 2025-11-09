CMPE 283 — Assignment 2: KVM VM-Exit Statistics

Student: Nikhil Sai Venkat Manam
Student id : 019107023
Repo: https://github.com/nikhilmanam02/linuxforgcloud  
Branch used for Assignment: `assignment2`

This repo contains a small KVM/VMX instrumentation that counts VM-exit reasons and prints a summary every 10,000 exits. Screenshots and logs are in `docs/a2/`.


 What I changed (where in the kernel)

 `arch/x86/kvm/vmx/exit_stats.h` — declarations for counters and helpers.
 `arch/x86/kvm/vmx/exit_stats.c` — implements:
 `atomic64_t` array indexed by exit reason (0..255)
  a global `atomic64_t` total counter
  a small name mapper for common reasons (CPUID, HLT, IO, MSR_READ, MSR_WRITE, EPT_VIOLATION, EXTERNAL_INTERRUPT), others show as `UNKNOWN`
  a “maybe dump” that prints a summary every 10,000 exits, omitting zero counts
 `arch/x86/kvm/Makefile` — adds `exit_stats.o` to the VMX objects.
 `arch/x86/kvm/vmx/vmx.c` — includes `exit_stats.h` and, at the **very top** of `__vmx_handle_exit(...)`, reads `VM_EXIT_REASON`, updates the counters, and calls the periodic dump helper.
 Placing the hook at the start of `__vmx_handle_exit()` ensures every exit is counted before any individual handler runs.


Answers to the assignment questions

 1) Team contributions
 Solo work

 2) Detailed steps I used

Steps I used to complete the assignment

Tested on outer VM cmpe283-outer (Ubuntu 22.04.5) running custom kernel 6.18.0-rc4-cmpe283+ with KVM enabled. Inner VM is Ubuntu 22.04.5 (kernel 5.15). Screenshots live under docs/a2/.

Verify environment on the OUTER VM
hostnamectl
uname -r
modinfo kvm_intel | egrep 'filename|vermagic'
lsmod | egrep '^kvm|kvm_intel'
ls -l /dev/kvm


Proof: docs/a2/outer-uname-hostnamect1.jpg, docs/a2/outer-modinfo-vermagic.jpg, docs/a2/outer-lsmod-kvm.jpg, docs/a2/outer-kvm-ok-devkvm.jpg.

1) Fork, clone, branch

Forked torvalds/linux to my account.

On the outer VM:

git clone https://github.com/nikhilmanam02/linuxforgcloud.git ~/linux
cd ~/linux
git remote add upstream https://github.com/torvalds/linux.git
git checkout -b assignment2

2) Implement KVM VM-exit stats (code edits)

Files changed/added:

arch/x86/kvm/vmx/vmx.c — hook the exit handler at the very start of __vmx_handle_exit():
Read VM_EXIT_REASON, mask to 16 bits, call cmpe283_count_exit(reason) and cmpe283_maybe_dump().
arch/x86/kvm/vmx/exit_stats.h — declare:
void cmpe283_count_exit(u32 reason);
void cmpe283_maybe_dump(void);

arch/x86/kvm/vmx/exit_stats.c — implement counters:
atomic64_t per-exit-type array (size 0x100) plus a global total.
Every 10,000 total exits: printk one line per non-zero exit with number, human-readable name, and count.
arch/x86/kvm/Makefile — add vmx/exit_stats.o to the VMX object list so it builds with KVM.


3) Build and install the updated KVM (VMX) bits

I built and installed just the VMX module subtree (faster than a full kernel), then reloaded modules:

cd ~/linux
grep ^CONFIG_KVM_INTEL .config   # expect CONFIG_KVM_INTEL=m
make -j"$(nproc)" M=arch/x86/kvm/vmx modules
sudo make M=arch/x86/kvm/vmx modules_install
sudo depmod -a
 reload (stop inner QEMU first if running)
pkill -TERM -f 'qemu-system-x86_64.*inner.qcow2' || true
sudo modprobe -r kvm_intel kvm || true
sudo modprobe kvm
sudo modprobe kvm_intel
dmesg | tail -n 30

4) Launch the INNER VM with cloud-init and SSH port-forward

I used Ubuntu jammy cloud image, a seed ISO, and QEMU user networking with host-forward on port 2222:

cd ~/inner
 seed.iso already created with user 'ubuntu' / password 'ubuntu'
qemu-system-x86_64 \
  -enable-kvm -cpu host -smp 2 -m 4096 \
  -drive file=jammy-server-cloudimg-amd64.img,if=virtio,format=qcow2,readonly=on \
  -drive file=inner.qcow2,if=virtio,format=qcow2 \
  -cdrom seed.iso \
  -netdev user,id=net0,hostfwd=tcp::2222-:22 \
  -device virtio-net-pci,netdev=net0 \
  -daemonize -pidfile inner.pid -display none -serial none -monitor none


Proof: docs/a2/qemu-port-2222.jpg.

5) Verify the inner VM is up (from OUTER) and SSH works

On the inner console: cloud-init status --wait, hostnamectl, systemctl status ssh --no-pager

Proof: docs/a2/inner-cloud-init-status.jpg, docs/a2/inner-hostnamect1.jpg, docs/a2/inner-ssh-active.jpg.
From the outer VM: ssh -p 2222 ubuntu@127.0.0.1 (password: ubuntu).

6) Generate exits inside the inner VM

I ran a tiny program that repeatedly executes CPUID for ~30s (causes lots of VM exits):
Built on the outer and copied in (inner / had low free space):
cat > ~/cpuidstorm.c <<'EOF'
#include <cpuid.h>
#include <time.h>
#include <stdio.h>
int main(){ struct timespec s,n; clock_gettime(CLOCK_MONOTONIC,&s);  unsigned a,b,c,d; unsigned long long it=0;
  do{ __get_cpuid(0,&a,&b,&c,&d); if(++it % 10000000==0) clock_gettime(CLOCK_MONOTONIC,&n); }
  while((n.tv_sec - s.tv_sec) < 30);
  printf("done: %llu iters\n", it);
  return 0;
}
EOF
gcc -O2 ~/cpuidstorm.c -o ~/cpuidstorm
scp -P 2222 ~/cpuidstorm ubuntu@127.0.0.1:/home/ubuntu/


On the inner VM:

chmod +x /home/ubuntu/cpuidstorm
sudo -u ubuntu -H /home/ubuntu/cpuidstorm
 output: done: ... iters. 

Proof: docs/a2/cpuidstorm-run.jpg.

7) Collect KVM printk output from the OUTER VM

The KVM changes print a dump every 10,000 exits and a final summary line occasionally:
dmesg -T | egrep -i 'cmpe283|exit' | tail -n 250 | tee docs/a2/dmesg-exit-dump.txt

Proof: docs/a2/dmesg-exit-dumps.jpg, docs/a2/dmesg-last-dump.jpg, and the text log docs/a2/dmesg-exit-dump.txt.


### 3) What can you say about the frequency of exits?
During inner VM boot: exit rate is high (EPT setup, MSR/I/O programming, interrupts) and then tapers off at the login prompt.
During the CPUID workload: exits increase at a steady, high rate; you can see the counter cross multiple 10k boundaries within seconds.
When idle: mostly occasional timer/interrupt‐related activity and `HLT`/wakeups.



## Screenshots & log (all under `docs/a2/`)
- `outer-uname-hostnamect1.jpg` — outer OS + kernel info
- `outer-kvm-ok-devkvm.jpg` — `/dev/kvm` sanity
- `outer-lsmod-kvm.jpg` — KVM modules loaded
- `outer-modinfo-vermagic.jpg` — `kvm_intel` vermagic matches
- `qemu-port-2222.jpg` — QEMU port forward
- `inner-hostnamect1.jpg` — inner VM hostname/OS
- `inner-cloud-init-status.jpg` — cloud-init completed
- `inner-ssh-active.jpg` — SSH active in inner VM
- `cpuidstorm-run.jpg` — workload run result (`done: … iters`)
- `dmesg-exit-dumps.jpg` — periodic exit dumps visible
- `dmesg-last-dump.jpg` — latest dump example
- `dmesg-exit-dump.txt` — tail of CMPE283 lines from `dmesg`



My Assignment 2 commits
(Branch: assignment2)
93ebcf933fa7e41f6af3d60d34beb1af82a40d52
f2a55b04cff094f1e90dfc8cdf04ae36111c2c78
24beab7350b49e7cfc5e572b8529585976dd947a
9e0929a9cd3744998c1a3ac5cc77075b6b7f8b1c
a1388fcb52fcad3e0b06e2cdd0ed757a82a5be30
