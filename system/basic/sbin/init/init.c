#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#include <vprintf.h>
#include <sysinfo.h>
#include <sys/klog.h>
#include <sys/ipc.h>
#include <sys/proc.h>
#include <dirent.h>

static int _console_fd = -1;

static void outc(char c, void* p) {
  str_t* buf = (str_t*)p;
  str_addc(buf, c);
}

static void out(const char *format, ...) {
  str_t* str = str_new(NULL);
  va_list ap;
  va_start(ap, format);
  v_printf(outc, str, format, ap);
	va_end(ap);
	klog("%s", str->cstr);
  str_free(str);;
}

static int check_file(const char* cmd_line) {
  str_t* cmd = str_new("");
  const char *p = cmd_line;
  while(*p != 0 && *p != ' ') {
    str_addc(cmd, *p);
    p++;
  }
  str_addc(cmd, 0);
	int res = vfs_access(cmd->cstr);
  str_free(cmd);
	return res;
}

static int run(const char* cmd, bool prompt, bool wait) {
	if(check_file(cmd) != 0) {
		out("init: file not found! [%s]\n", cmd);
		return -1;
	}

	if(prompt)
		out("init: %s ", cmd);

	int pid = fork();
	if(pid == 0) {
		if(_console_fd > 0) {
			close(_console_fd);
			_console_fd = -1;
		}

		proc_detach();
		if(exec(cmd) != 0) {
			if(prompt)
				out("[error!]\n");
			exit(-1);
		}
	}
	else if(wait)
		proc_wait_ready(pid);

	if(prompt)
		out("[ok]\n");
	return 0;
}

static const char* read_line(int fd) {
	static char line[256];
	int i = 0;
	while(i < 255) {
		char c;
		if(read(fd, &c, 1) <= 0) {
			if(i == 0)
				return NULL;
			else
				break;
		}	

		if(c == '\n')
			break;
		line[i] = c;
		i++;
	}
	line[i] = 0;
	return line;
}

static void load_devs(const char* cfg) {
	int fd = open(cfg, O_RDONLY);
	if(fd < 0)
		return;

	while(true) {
		const char* ln = read_line(fd);
		if(ln == NULL)
			break;
		if(ln[0] == 0 || ln[0] == '#')
			continue;
		run(ln, true, true);
	}
	close(fd);
}

static void load_arch_devs(void) {
	sys_info_t sysinfo;
	syscall1(SYS_GET_SYS_INFO, (int32_t)&sysinfo);
	char fn[FS_FULL_NAME_MAX];
	snprintf(fn, FS_FULL_NAME_MAX-1, "/etc/dev/arch/%s/init.dev", sysinfo.machine);
	load_devs(fn);
}

static void load_extra_devs(void) {
	const char* dirn = "/etc/dev/extra";
  DIR* dirp = opendir(dirn);
  if(dirp == NULL)
    return;
  while(1) {
    struct dirent* it = readdir(dirp);
    if(it == NULL)
      break;
		char fn[FS_FULL_NAME_MAX];
    snprintf(fn, FS_FULL_NAME_MAX-1, "%s/%s", dirn, it->d_name);
		load_devs(fn);
  }
  closedir(dirp);
}

static void console_welcome(void) {
	dprintf(_console_fd,
			" ______           ______  _    _   ______  ______ \n"
			"(  ___ \\|\\     /|(  __  )| \\  / \\ (  __  )(  ___ \\\n"
			"| (__   | | _ | || |  | || (_/  / | |  | || (____\n"
			"|  __)  | |( )| || |  | ||  _  (  | |  | |(____  )\n"
			"| (___  | || || || |__| || ( \\  \\ | |__| |  ___) |\n"
			"(______/(_______)(______)|_/  \\_/ (______)\\______)\n\n");
}

static void load_sys_init_devs(void) {
	load_devs("/etc/dev/sys_init.dev");
	_console_fd = open("/dev/console0", O_WRONLY);
	if(_console_fd > 0) 
		console_welcome();
}

static void load_sys_devs(void) {
	load_devs("/etc/dev/sys.dev");
}

static void run_procs(void) {
	int fd = open("/etc/init.rd", O_RDONLY);
	if(fd < 0)
		return;

	while(true) {
		const char* ln = read_line(fd);
		if(ln == NULL)
			break;
		if(ln[0] == 0 || ln[0] == '#')
			continue;
		run(ln, false, false);
	}
	close(fd);
}

void core(void);
static void run_core(void) {
	out("run init-core    ");
	int pid = fork();
	if(pid == 0) {
		syscall1(SYS_PROC_SET_CMD, (int32_t)"init-core");
		core();
	}
	else
		proc_wait_ready(pid);
	out("[ok]\n");
}

int vfsd_main(void);
static void run_vfsd(void) {
	out("run init-vfsd    ");
	int pid = fork();
	if(pid == 0) {
		syscall1(SYS_PROC_SET_CMD, (int32_t)"init-vfsd");
		vfsd_main();
	}
	else
		proc_wait_ready(pid);
	out("[ok]\n");
}

int procd_main(void);
static void run_procd(void) {
	out("run init-procd    ");
	int pid = fork();
	if(pid == 0) {
		syscall1(SYS_PROC_SET_CMD, (int32_t)"init-procd");
		procd_main();
	}
	else
		proc_wait_ready(pid);
	out("[ok]\n");
}

void romfsd_main(void);
void sdfsd_main(void);
static void init_rootfs(void) {
	out("run init-rootfsd    ");
	int pid = fork();
	if(pid == 0) {
		int32_t sz = syscall3(SYS_KROMFS_GET, 0, (int32_t)NULL, (int32_t)NULL);	
		if(sz < 0) {
			syscall1(SYS_PROC_SET_CMD, (int32_t)"init-sdfsd");
			sdfsd_main();
		}
		else {
			syscall1(SYS_PROC_SET_CMD, (int32_t)"init-romfsd");
			romfsd_main();
		}
	}
	else
		proc_wait_ready(pid);
	out("[ok]\n");
}

static void init_tty_stdio(void) {
	int fd = open("/dev/tty0", 0);
	dup2(fd, 0);
	dup2(fd, 1);
	if(_console_fd > 0) {
		dup2(_console_fd, 2);
		close(_console_fd);
		_console_fd = -1;
	}
	else
		dup2(fd, 2);
	close(fd);
}

static void switch_root(void) {
	int pid = fork();
	if(pid == 0) {
		setuid(0);
		load_arch_devs();
		load_extra_devs();
		load_sys_init_devs();
		init_tty_stdio();
		load_sys_devs();
		run_procs();
		exit(0);
	}
}

int main(int argc, char** argv) {
	(void)argc;
	(void)argv;
	_console_fd = -1;

	if(getuid() >= 0) {
		out("process 'init' can only loaded by kernel!\n");
		return -1;
	}

	out("\n[init process started]\n");
	syscall1(SYS_PROC_SET_CMD, (int32_t)"init");
	run_core();
	run_procd();
	run_vfsd();

	//load procs before file system ready
	init_rootfs();
	switch_root();

	while(true) {
		proc_block(getpid(), (uint32_t)main);
	}
	return 0;
}
