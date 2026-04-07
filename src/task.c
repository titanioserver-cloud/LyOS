#include "../include/task.h"
#include "../include/kernel.h"
#include "../include/gdt.h"
#include "../include/paging.h"

#define MAX_TASKS 16
#define MAX_MSGS 16

typedef struct { int sender_pid; int type; int data1; int data2; } ipc_msg_t;

uint8_t kernel_stacks[MAX_TASKS][8192] __attribute__((aligned(16)));
uint64_t task_rsps[MAX_TASKS];
uint64_t task_cr3s[MAX_TASKS];
int task_states[MAX_TASKS]; 
ipc_msg_t msg_queues[MAX_TASKS][MAX_MSGS];
int msg_counts[MAX_TASKS];

int num_tasks = 0;
int current_task = -1;

int create_task(void (*entry)()) {
    if (num_tasks >= MAX_TASKS) return -1;
    int pid = num_tasks;
    
    uint64_t pml4 = create_address_space();
    task_cr3s[pid] = pml4;
    
    uint64_t user_stack_vaddr = 0x00007FFFF0000000;
    map_user_memory(pml4, user_stack_vaddr - 16384, 16384);
    
    uint64_t* stack = (uint64_t*)&kernel_stacks[pid][8192];
    *(--stack) = 0x23; *(--stack) = user_stack_vaddr;
    *(--stack) = 0x202; *(--stack) = 0x1B; *(--stack) = (uint64_t)entry;
    *(--stack) = 0; *(--stack) = 32;
    for(int i = 0; i < 15; i++) { *(--stack) = 0; }
    
    task_rsps[pid] = (uint64_t)stack; 
    task_states[pid] = 1; 
    msg_counts[pid] = 0;
    num_tasks++;
    return pid;
}

void kill_task(int pid) { if (pid >= 0 && pid < num_tasks) task_states[pid] = 0; }

int send_msg(int to_pid, int type, int d1, int d2) {
    if (to_pid < 0 || to_pid >= num_tasks || task_states[to_pid] == 0) return -1;
    if (msg_counts[to_pid] >= MAX_MSGS) return -2;
    int tail = msg_counts[to_pid];
    msg_queues[to_pid][tail].sender_pid = current_task;
    msg_queues[to_pid][tail].type = type;
    msg_queues[to_pid][tail].data1 = d1;
    msg_queues[to_pid][tail].data2 = d2;
    msg_counts[to_pid]++;
    return 0;
}

int recv_msg(int* type, int* d1, int* d2) {
    if (current_task < 0 || msg_counts[current_task] == 0) return -1;
    ipc_msg_t msg = msg_queues[current_task][0];
    for (int i = 0; i < msg_counts[current_task] - 1; i++) msg_queues[current_task][i] = msg_queues[current_task][i + 1];
    msg_counts[current_task]--;
    *type = msg.type; *d1 = msg.data1; *d2 = msg.data2;
    return msg.sender_pid;
}

void pit_init() { outb(0x43, 0x36); uint16_t divisor = 11931; outb(0x40, (uint8_t)(divisor & 0xFF)); outb(0x40, (uint8_t)((divisor >> 8) & 0xFF)); }
void tasking_init() { pit_init(); }

int get_num_tasks() { int count = 0; for(int i = 0; i < num_tasks; i++) { if(task_states[i] == 1) count++; } return count; }

uint64_t timer_tick(uint64_t rsp) {
    if (num_tasks == 0) return rsp;
    if (current_task >= 0 && task_states[current_task] == 1) task_rsps[current_task] = rsp;
    
    int next_task = (current_task + 1) % num_tasks;
    int tries = 0;
    while (task_states[next_task] == 0 && tries < num_tasks) { next_task = (next_task + 1) % num_tasks; tries++; }
    if (tries == num_tasks) return rsp;
    
    current_task = next_task;
    update_tss_rsp0((uint64_t)&kernel_stacks[current_task][8192]);
    switch_address_space(task_cr3s[current_task]);
    return task_rsps[current_task];
}