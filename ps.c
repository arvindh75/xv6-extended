#include "types.h"
#include "stat.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "user.h"
#include "fs.h"

int ps() {
    int ret = ps_func();
    return ret;
}

int main(int argc, char *argv[]) {
    ps();
    exit();
}
