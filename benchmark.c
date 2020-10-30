#include "types.h"
#include "user.h"

int number_of_processes = 5;

int main(int argc, char *argv[])
{
    int j;
    for (j = 0; j < number_of_processes; j++)
    {
        sleep(10);
        int pid = fork();
        if (pid < 0)
        {
            printf(1, "Fork failed\n");
            continue;
        }
        if (pid == 0)
        {
            volatile int i;
            for (volatile int k = 0; k < number_of_processes; k++)
            {
                if (k <= j)
                {
                    sleep(50); //io time
                }
                else
                {
                    for (i = 0; i < 100000000; i++)
                    {
                        int y = i* 31255 * 2; //cpu time
                        y++;
                    }
                }
            }
            printf(1, "Process: %d Finished\n", j);
            exit();
        }
        else{
            set_priority(100-(20+j),pid); // will only matter for PBS, comment it out if not implemented yet (better priorty for more IO intensive jobs)
        }
    }
    for (j = 0; j < number_of_processes+5; j++)
    {
        wait();
    }
    exit();
}
