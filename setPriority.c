#include "types.h"
#include "user.h"

int setPriority(int new_priority, int pid) {
    int ret = set_priority(new_priority, pid);
    return ret;
}

int main(int argc, char *argv[]) {
    if(argc < 3){
        exit();
    }
    setPriority(atoi(argv[1]), atoi(argv[2]));
    exit();
}
