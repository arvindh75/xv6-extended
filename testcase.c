#include "types.h"
#include "user.h"

void AdvancedBenchmark(int n){
    int j;
    for (j = 0; j < n; j++)
    {
        int pid = fork();
        if (pid < 0)
        {
            printf(1, "Fork failed\n");
            continue;
        }
        if (pid == 0)
        {
            volatile int i;
            for (volatile int k = 0; k < n; k++)
            {
                if (k <= j)
                {
                    sleep(200 +j*10+k*5); //io time
                    i=i^1;
                }
                else
                {
                    for (i = 0; i < 100000000; i++)
                    {
                        if(i%1000000==0){
                            sleep(10);
                        }
                        double x =  x + 3.14 * 89.64;
                    }
                }
            }
            //if(!PLOT)
            printf(1, "Process: %d Finished\n", j);
            exit();
        }
        else{
            ;
            set_priority(100-(20+j),pid); // will only matter for PBS, comment it out if not implemented yet (better priorty for more IO intensive jobs)
        }
    }
    for (j = 0; j < n+5; j++)
    {
        wait();
    }
}

void benchmark(int n){
    int j;
    for (j = 0; j < n; j++)
    {
        int pid = fork();
        if (pid < 0)
        {
            printf(1, "Fork failed\n");
            continue;
        }
        if (pid == 0)
        {
            volatile int i;
            for (volatile int k = 0; k < n; k++)
            {
                if (k <= j)
                {
                    sleep(200); //io time
                }
                else
                {
                    for (i = 0; i < 100000000; i++)
                    {
                        ;
                    }
                }
            }
            //if(!PLOT)
            printf(1, "Process: %d Finished\n", j);
            exit();
        }
        else{
            ;
            set_priority(100-(20+j),pid); // will only matter for PBS, comment it out if not implemented yet (better priorty for more IO intensive jobs)
        }
    }
    for (j = 0; j < n+5; j++)
    {
        wait();
    }
}

void cpuBound(int n) {
    for (int i = 0; i < n; i++){
        int pid = fork();
        if (pid == 0) {
            for (double z = 0; z < 1000000.0; z+= 0.01){
                double x =  x + 3.14 * 89.64;   // useless calculations to consume CPU time
            }
            //if(!PLOT)
            printf(1, "Process: %d Finished\n", i);
            exit(); //bye bye
        }
        continue;
    }
    while(wait()!=-1);
}

void IoBound(int n) {
    for (int i = 0; i < n; i++){
        int pid = fork();
        if (pid == 0) {
            for(int k = 0; k < i*100+1; k++){
                sleep((10*(n-i))%3+1);
            }
            //if(!PLOT)
            printf(1, "Process: %d Finished\n", i);
            exit(); //bye bye
        }
        continue;
    }
    while(wait()!=-1);
}

void IntelligentProcess(int n) {//you might want to tweak the sleep intervals.. according to timing of desired queue..

    for (int i = 0; i < n; i++){
        int pid = fork();
        if (pid == 0) {
            int j=0;
            for (double z = 0; z < 1000000.0; z+= 0.01,j++){
                double x =  x + 3.14 * 89.64;   // useless calculations to consume CPU time
                if(i==0){ //this guy is damn intelligent imo
                    if(j%100000==0){
                        sleep(1);
                    }
                }
            }
            //if(!PLOT)
            printf(1, "Process: %d Finished\n", i);
            exit(); //bye bye
        }
        continue;
    }
    while(wait()!=-1);
}

void Mixture(int n) {
    int pid;
    int j;
    int k;
    for (int i = 0; i < n; i++) {
        j = i % 3;
        sleep(25);
        pid = fork();
        if (pid == 0) {
            j = (getpid() - 4) % 2;
            switch(j) {
                case 0: //CPU‐bound process (CPU):
                    for (double z = 0; z < 1000000.0; z+= 0.01){
                        double x =  x + 3.14 * 89.64;   // useless calculations to consume CPU time
                    }
                    break;
                case 1:// simulate I/O bound process (IO)
                    for(k = 0; k < i*10+1; k++){
                        sleep((10 * (n-i))%(9)+1);
                    }
                    break;
            }
            //if(!PLOT)
            printf(1, "Process: %d Finished\n", i);
            exit(); //bye bye
        }
        continue; 
    }
    while(wait()!=-1);
}

void ShowFailureofFCFS(int n) {
    int pid;
    for(int i=0; i<n; i++){
        pid=fork();
        if(pid==0){//child
            if(i>n/2){
                for(int k = 0; k < i*10+1; k++){
                    sleep((10 * (n-i))%(9)+1);
                }
            }
            else{
                for(double z = 0; z < (i%2+1)*1.5*1000000.0; z+= 0.01){
                    double x =  x + 3.14 * 89.64;   // useless calculations to consume CPU time
                }
            }
            //if(!PLOT)
            printf(1, "Process: %d Finished\n", i);
            exit(); //bye bye
        }
        else{
            if(i<n/2)
                set_priority((60-(10*i)%50),pid); //win-win for PBS
        }
    }
    while(wait()!=-1);
}

int main(int argc, char *argv[]) {
    if(argc!=3){
        printf(1,"Usage <Test Type Number> <Number of Procs>\n");
        exit();
    }
    int n = atoi(argv[2]);
    int test = atoi(argv[1]);
    switch(test){
        case 1: benchmark(n);//normal benchmark
                break;
        case 2: cpuBound(n);//benchmark for n CPU Bound Processes
                break;
        case 3: IoBound(n);//n IO Bound Processes
                break;
        case 4: IntelligentProcess(n); //The intelligent Process test... A test which gives up CPU during its calcs to stay in higher Queue.. Only useful for MLFQ Testing
                // n-1 procecess are stupid and 1 is intelligent..
                break;
        case 5: Mixture(n); //Mixture of IO Bound  and CPU Bound Process
                break;
        case 6: AdvancedBenchmark(n); // A modified version of given Benchmark 
                break;
        case 7: ShowFailureofFCFS(n);// Here we show why FCFS fails if many Process IO Heavy and they come first. First n/2 are IO bound and n-n/2 is CPU bound but IO is made first
                break;
    }
    exit();
}
