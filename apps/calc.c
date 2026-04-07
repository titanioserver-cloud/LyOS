#include "../lib/protos.h"

int main() {
    int total = 0;
    while(1) {
        yield(); 
        int type, d1, d2;
        int sender = recv_msg(&type, &d1, &d2);
        if (sender > 0) {
            if (type == 1) { 
                total += d1;
                send_msg(sender, 2, total, 0); 
            } else if (type == 3) { 
                total = 0;
                send_msg(sender, 2, total, 0);
            }
        }
    }
    return 0;
}