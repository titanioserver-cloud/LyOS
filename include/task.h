#ifndef TASK_H
#define TASK_H
#include <stdint.h>

void tasking_init();
int create_task(void (*entry)());
int get_num_tasks();
void kill_task(int pid);
int send_msg(int to_pid, int type, int d1, int d2);
int recv_msg(int* type, int* d1, int* d2);

#endif