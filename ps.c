#include "types.h"
#include "stat.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "user.h"
#include "fs.h"

int ps() {
    struct proc_ps *p1[NPROC];
    int ret = ps_func(&p1);
    return ret;
}

int main(int argc, char *argv[]) {
    ps();
    exit();
}
