#include <procinfo.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sysinfo.h>
#include <string.h>

static const char* _states[] = {
	"unu",
	"crt",
	"slp",
	"wat",
	"blk",
	"rdy",
	"run",
	"zmb"
};

static const char* get_cmd(char* cmd, int full) {
	if(full)
		return cmd;

	char* p = cmd;
	while(*p != 0) {
		if(*p == ' ') {
			*p = 0;
			break;
		}
		p++;
	}
	return cmd;
}

int main(int argc, char* argv[]) {
	int full = 0;
	int all = 0;
	if(argc == 2 && argv[1][0] == '-') {	
		if(strchr(argv[1], 'f') != NULL)
			full = 1;
		if(strchr(argv[1], 'a') != NULL)
			all = 1;
	}

	int num = 0;
	sysinfo_t sysinfo;
	syscall1(SYS_GET_SYSINFO, (int32_t)&sysinfo);
	uint32_t fr_mem = sysinfo.free_mem / (1024*1024);
	uint32_t shm_mem = sysinfo.shm_mem / (1024*1024);
	uint32_t t_mem = sysinfo.total_mem / (1024*1024);
	uint32_t csec = sysinfo.kernel_sec;

	procinfo_t* procs = (procinfo_t*)syscall1(SYS_GET_PROCS, (int)&num);
	if(procs != NULL) {
		printf("  PID    FATHER OWNER   STATE IPCS TIME       PROC\n"); 
		for(int i=0; i<num; i++) {
			if(procs[i].type != PROC_TYPE_PROC && all == 0)
				continue;

			uint32_t sec = csec - procs[i].start_sec;
			printf("  %4d   %6d %5d   %5s %4d %02d:%02d:%02d   %s", 
				procs[i].pid,
				procs[i].father_pid,
				procs[i].owner,
				_states[procs[i].state],
				procs[i].ipc_tasks,
				sec / (3600),
				sec / 60,
				sec % 60,
				get_cmd(procs[i].cmd, full));
			
			int blk = procs[i].block_by;
			if(procs[i].state == 4) {
				if(blk == procs[i].pid)
					printf(" : wat_ipc_call\n");
				else if(blk < 0)
					printf(" : blk_by kernel\n");
				else
					printf(" : blk_by %d:%s\n", blk, get_cmd(procs[procs[i].block_by].cmd, full));
			}
			else
				printf("\n");
		}
		free(procs);
	}
	printf("  memory: total %d MB, free %d MB, shm %d MB\n", t_mem, fr_mem, shm_mem);
	printf("  processes: %d\n", num);
	return 0;
}
