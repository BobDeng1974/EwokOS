#include <kernel/kernel.h>
#include <kernel/svc.h>
#include <kernel/schedule.h>
#include <kernel/system.h>
#include <kernel/proc.h>
#include <kernel/uspace_int.h>
#include <kernel/hw_info.h>
#include <kernel/kevqueue.h>
#include <mm/kalloc.h>
#include <mm/shm.h>
#include <mm/kmalloc.h>
#include <sysinfo.h>
#include <dev/framebuffer.h>
#include <dev/uart.h>
#include <dev/timer.h>
#include <syscalls.h>
#include <kstring.h>
#include <kprintf.h>
#include <buffer.h>
#include <rawdata.h>
#include <dev/sd.h>

static void sys_kprint(const char* s, int32_t len, bool tty_only) {
	(void)len;
	if(tty_only)
		uart_write(s, strlen(s));
	else
		printf(s);
}

static void sys_exit(context_t* ctx, int32_t res) {
	if(_current_proc == NULL)
		return;
	proc_exit(ctx, _current_proc, res);
}

static int32_t sys_sdc_read(int32_t bid) {
	return sd_dev_read(bid);
}

static int32_t sys_sdc_write(int32_t bid, const char* buf) {
	return sd_dev_write(bid, buf);
}

static void sys_sdc_read_done(context_t* ctx, void* buf) {
	int res = sd_dev_read_done(buf);
	if(res == 0) {
		ctx->gpr[0] = res;
		return;
	}
	ctx->gpr[0] = -1;
}

static void sys_sdc_write_done(context_t* ctx) {
	int res = sd_dev_write_done();
	if(res == 0) {
		ctx->gpr[0] = res;
		return;
	}
	ctx->gpr[0] = -1;
}

static int32_t sys_getpid(int32_t pid) {
	proc_t * proc = _current_proc;
	if(pid >= 0)
		proc = proc_get(pid);

	if(proc == NULL)
		return -1;

	if(_current_proc->owner > 0 && _current_proc->owner != proc->owner)
		return -1;

	proc_t* p = proc_get_proc(proc);
	if(p != NULL)
		return p->pid;
	return -1;
}

static int32_t sys_get_threadid(void) {
	if(_current_proc == NULL || _current_proc->type != PROC_TYPE_THREAD)
		return -1;
	return _current_proc->pid; 
}


static void sys_usleep(context_t* ctx, uint32_t count) {
	proc_usleep(ctx, count);
}

static void sys_kill(context_t* ctx, int32_t pid) {
	proc_t* proc = proc_get(pid);
	if(proc == NULL)
		return;

	if(_current_proc->owner == 0 || proc->owner == _current_proc->owner) {
		proc_exit(ctx, proc, 0);
	}
}

static int32_t sys_malloc(int32_t size) {
	return (int32_t)proc_malloc(size);
}

static int32_t sys_realloc(void* p, int32_t size) {
	return (int32_t)proc_realloc(p, size);
}

static void sys_free(int32_t p) {
	if(p == 0)
		return;
	proc_free((void*)p);
}

static void sys_fork(context_t* ctx) {
	proc_t *proc = kfork(PROC_TYPE_PROC);
	if(proc == NULL) {
		ctx->gpr[0] = -1;
		return;
	}

	memcpy(&proc->ctx, ctx, sizeof(context_t));
	proc->ctx.gpr[0] = 0;
	ctx->gpr[0] = proc->pid;
	if(proc->state == CREATED) {
		_current_proc->state = BLOCK;
		_current_proc->block_pid = _core_pid;
		_current_proc->block_event = proc->pid;

		proc->state = BLOCK;
		proc->block_pid = _core_pid;
		proc->block_event = proc->pid;
		schedule(ctx);
	}
}

static void sys_detach(void) {
	_current_proc->father_pid = 0;
}

static int32_t sys_thread(context_t* ctx, uint32_t entry, uint32_t func, int32_t arg) {
	proc_t *proc = kfork(PROC_TYPE_THREAD);
	if(proc == NULL)
		return -1;
	uint32_t sp = proc->ctx.sp;
	memcpy(&proc->ctx, ctx, sizeof(context_t));
	proc->ctx.sp = sp;
	proc->ctx.pc = entry;
	proc->ctx.lr = entry;
	proc->ctx.gpr[0] = func;
	proc->ctx.gpr[1] = arg;
	return proc->pid;
}

static void sys_waitpid(context_t* ctx, int32_t pid) {
	proc_waitpid(ctx, pid);
}

static void sys_load_elf(context_t* ctx, const char* cmd, void* elf, uint32_t elf_size) {
	if(elf == NULL) {
		ctx->gpr[0] = -1;
		return;
	}

	str_cpy(_current_proc->cmd, cmd);
	if(proc_load_elf(_current_proc, elf, elf_size) != 0) {
		ctx->gpr[0] = -1;
		return;
	}

	ctx->gpr[0] = 0;
	memcpy(ctx, &_current_proc->ctx, sizeof(context_t));
}

static int32_t sys_proc_set_cwd(const char* cwd) {
	str_cpy(_current_proc->cwd, cwd);
	return 0;
}

static int32_t sys_proc_set_uid(int32_t uid) {
	if(_current_proc->owner != 0)	
		return -1;
	_current_proc->owner = uid;
	return 0;
}

static void sys_proc_get_cwd(char* cwd, int32_t sz) {
	strncpy(cwd, CS(_current_proc->cwd), sz);
}

static void sys_proc_get_cmd(int32_t pid, char* cmd, int32_t sz) {
	strncpy(cmd, CS(proc_get(pid)->cmd), sz);
}

static void	sys_get_sysinfo(sysinfo_t* info) {
	if(info == NULL)
		return;

	strcpy(info->machine, get_hw_info()->machine);
	info->free_mem = get_free_mem_size();
	info->total_mem = get_hw_info()->phy_mem_size;
	info->shm_mem = shm_alloced_size();
	info->kernel_tic = _kernel_tic;
}

static int32_t sys_get_env(const char* name, char* value, int32_t size) {
	const char* v = proc_get_env(name);
	if(v == NULL)
		value[0] = 0;
	else
		strncpy(value, v, size);
	return 0;
}

static int32_t sys_get_env_name(int32_t index, char* name, int32_t size) {
	const char* n = proc_get_env_name(index);
	if(n[0] == 0)
		return -1;
	strncpy(name, n, size);
	return 0;
}

static int32_t sys_get_env_value(int32_t index, char* value, int32_t size) {
	const char* v = proc_get_env_value(index);
	if(v == NULL)
		value[0] = 0;
	else
		strncpy(value, v, size);
	return 0;
}

static int32_t sys_shm_alloc(uint32_t size, int32_t flag) {
	return shm_alloc(size, flag);
}

static void* sys_shm_map(int32_t id) {
	return shm_proc_map(_current_proc->pid, id);
}

static int32_t sys_shm_unmap(int32_t id) {
	return shm_proc_unmap(_current_proc->pid, id);
}

static int32_t sys_shm_ref(int32_t id) {
	return shm_proc_ref(_current_proc->pid, id);
}
	
static uint32_t sys_mmio_map(void) {
	if(_current_proc->owner != 0)
		return 0;
	hw_info_t* hw_info = get_hw_info();
	map_pages(_current_proc->space->vm, MMIO_BASE, hw_info->phy_mmio_base, hw_info->phy_mmio_base + hw_info->mmio_size, AP_RW_RW);
	return MMIO_BASE;
}
		
static int32_t sys_framebuffer_map(fbinfo_t* info) {
	if(_current_proc->owner != 0)
		return 0;
	fbinfo_t *fbinfo = fb_get_info();
	memcpy(info, fbinfo, sizeof(fbinfo_t));
	map_pages(_current_proc->space->vm, fbinfo->pointer, V2P(fbinfo->pointer), V2P(info->pointer)+info->size, AP_RW_RW);
	return 0;
}

static void sys_proc_critical_enter(void) {
	if(_current_proc->owner != 0)
		return;
	_current_proc->critical_counter = CRITICAL_MAX;
}
	
static void sys_proc_critical_quit(void) {
	_current_proc->critical_counter = 0;
}

static int32_t sys_ipc_setup(context_t* ctx, uint32_t entry, uint32_t extra_data, bool prefork) {
	return proc_ipc_setup(ctx, entry, extra_data, prefork);
}

static void sys_ipc_call(context_t* ctx, uint32_t pid, int32_t call_id, proto_t* data) {
	ctx->gpr[0] = 0;
	proc_t* proc = proc_get(pid);
	if(proc->space->ipc.entry == 0) {
		ctx->gpr[0] = -2;
		return;
	}
	if(proc->space->ipc.state != IPC_IDLE) {
		ctx->gpr[0] = -1;
		//printf("ipc retry: from: %d, to:%d, call: %d\n", _current_proc->pid, pid, call_id);
		proc_block_on(ctx, (uint32_t)&proc->space->ipc.state);
		return;
	}
	proc->space->ipc.state = IPC_BUSY;
	proc->space->ipc.from_pid = _current_proc->pid;
	if(data != NULL)
		PF->copy(proc->space->ipc.data, data->data, data->size);
	else
		PF->clear(proc->space->ipc.data);
	proc_ipc_call(ctx, proc, call_id);
}

static void sys_ipc_get_return(context_t* ctx, uint32_t pid, proto_t* data) {
	ctx->gpr[0] = 0;
	proc_t* proc = proc_get(pid);
	if(proc->space->ipc.entry == 0 ||
			proc->space->ipc.from_pid != _current_proc->pid ||
			proc->space->ipc.state == IPC_IDLE) {
		ctx->gpr[0] = -2;
		return;
	}
	if(proc->space->ipc.state != IPC_RETURN) {
		ctx->gpr[0] = -1;
		proc_block_on(ctx, (uint32_t)&proc->space->ipc.data);
		return;
	}

	if(data != NULL) {
		data->total_size = data->size = proc->space->ipc.data->size;
		if(data->size > 0) {
			data->data = (proto_t*)proc_malloc(proc->space->ipc.data->size);
			memcpy(data->data, proc->space->ipc.data->data, data->size);
		}
	}
	PF->clear(proc->space->ipc.data);
	proc->space->ipc.state = IPC_IDLE;
	proc_wakeup(-1, (uint32_t)&proc->space->ipc.state);
}

static void sys_ipc_set_return(proto_t* data) {
	//if(_current_proc->type != PROC_TYPE_IPC ||
	if(_current_proc->space->ipc.entry == 0 ||
			_current_proc->space->ipc.state != IPC_BUSY) {
		return;
	}

	if(data != NULL)
		PF->copy(_current_proc->space->ipc.data, data->data, data->size);
}

static void sys_ipc_end(context_t* ctx) {
	//if(_current_proc->type != PROC_TYPE_IPC ||
	if(_current_proc->space->ipc.entry == 0 ||
			_current_proc->space->ipc.state != IPC_BUSY) {
		return;
	}

	_current_proc->space->ipc.state = IPC_RETURN;
	proc_wakeup(-1, (uint32_t)&_current_proc->space->ipc.data);
	_current_proc->state = BLOCK;
	schedule(ctx);
}

static int32_t sys_ipc_get_arg(void) {
	if(_current_proc->space->ipc.entry == 0 ||
			_current_proc->space->ipc.state != IPC_BUSY) {
		return 0;
	}

	proto_t* ret = NULL;
	if(_current_proc->space->ipc.data->size > 0) {
		ret = (proto_t*)proc_malloc(sizeof(proto_t));
		memset(ret, 0, sizeof(proto_t));
		ret->data = proc_malloc(_current_proc->space->ipc.data->size);
		ret->total_size = ret->size = _current_proc->space->ipc.data->size;
		memcpy(ret->data, _current_proc->space->ipc.data->data, _current_proc->space->ipc.data->size);
	}
	return (int32_t)ret;
}

static int32_t sys_proc_ping(int32_t pid) {
	proc_t* proc = proc_get(pid);
	if(proc == NULL || !proc->space->ready_ping)
		return -1;
	return 0;
}

static void sys_proc_ready_ping(void) {
	_current_proc->space->ready_ping = true;
}

static kevent_t* sys_get_kevent_raw(void) {
	if(_current_proc->owner != 0)	
		return NULL;

	kevent_t* kev = kev_pop();
	if(kev == NULL) {
		return NULL;
	}

	kevent_t* ret = (kevent_t*)proc_malloc(sizeof(kevent_t));
	ret->type = kev->type;
	if(kev->data != NULL && kev->data->size > 0) {
		ret->data = (proto_t*)proc_malloc(sizeof(proto_t));
		memset(ret->data, 0, sizeof(proto_t));
		ret->data->data = proc_malloc(kev->data->size);
		ret->data->total_size = ret->data->size = kev->data->size;
		memcpy(ret->data->data, kev->data->data, kev->data->size);
	}

	if(ret->data != NULL)
		proto_free(kev->data);
	kfree(kev);
	return ret;
}

static void sys_get_kevent(context_t* ctx) {
	ctx->gpr[0] = 0;	
	kevent_t* kev = sys_get_kevent_raw();
	if(kev == NULL) {
		proc_block_on(ctx, (uint32_t)kev_init);
		return;
	}
	ctx->gpr[0] = (int32_t)kev;	
}

static void sys_proc_block(context_t* ctx, int32_t pid, uint32_t evt) {
	if(pid == _current_proc->pid)
		return;
	_current_proc->state = BLOCK;
	_current_proc->block_event = evt;
	_current_proc->block_pid = pid;
	schedule(ctx);	
}

static void sys_proc_wakeup(uint32_t evt) {
	proc_wakeup(_current_proc->pid, evt);
}

static void sys_core_ready(void) {
	if(_current_proc->owner != 0)
		return;
	_core_ready = true;
	_core_pid = _current_proc->pid;
}

void svc_handler(int32_t code, int32_t arg0, int32_t arg1, int32_t arg2, context_t* ctx, int32_t processor_mode) {
	(void)arg1;
	(void)arg2;
	(void)ctx;
	(void)processor_mode;
	__irq_disable();
	_current_ctx = ctx;

	switch(code) {
	case SYS_SDC_READ:
		ctx->gpr[0] = sys_sdc_read(arg0);
		return;
	case SYS_SDC_WRITE:
		ctx->gpr[0] = sys_sdc_write(arg0, (void*)arg1);		
		return;
	case SYS_SDC_READ_DONE:
		sys_sdc_read_done(ctx, (void*)arg0);
		return;
	case SYS_SDC_WRITE_DONE:
		sys_sdc_write_done(ctx);
		return;
	case SYS_EXIT:
		sys_exit(ctx, arg0);
		return;
	case SYS_MALLOC:
		ctx->gpr[0] = sys_malloc(arg0);
		return;
	case SYS_REALLOC:
		ctx->gpr[0] = sys_realloc((void*)arg0, arg1);
		return;
	case SYS_FREE:
		sys_free(arg0);
		return;
	case SYS_GET_PID:
		ctx->gpr[0] = sys_getpid(arg0);
		return;
	case SYS_GET_THREAD_ID:
		ctx->gpr[0] = sys_get_threadid();
		return;
	case SYS_USLEEP:
		sys_usleep(ctx, (uint32_t)arg0);
		return;
	case SYS_KILL:
		sys_kill(ctx, arg0);
		return;
	case SYS_EXEC_ELF:
		sys_load_elf(ctx, (const char*)arg0, (void*)arg1, (uint32_t)arg2);
		return;
	case SYS_FORK:
		sys_fork(ctx);
		return;
	case SYS_DETACH:
		sys_detach();
		return;
	case SYS_WAIT_PID:
		sys_waitpid(ctx, arg0);
		return;
	case SYS_YIELD: 
		schedule(ctx);
		return;
	case SYS_PROC_SET_CWD: 
		ctx->gpr[0] = sys_proc_set_cwd((const char*)arg0);
		return;
	case SYS_PROC_GET_CWD: 
		sys_proc_get_cwd((char*)arg0, arg1);
		return;
	case SYS_PROC_SET_UID: 
		ctx->gpr[0] = sys_proc_set_uid(arg0);
		return;
	case SYS_PROC_GET_UID: 
		ctx->gpr[0] = _current_proc->owner;
		return;
	case SYS_PROC_GET_CMD: 
		sys_proc_get_cmd(arg0, (char*)arg1, arg2);
		return;
	case SYS_GET_SYSINFO:
		sys_get_sysinfo((sysinfo_t*)arg0);
		return;
	/*case SYS_GET_KERNEL_USEC:
		sys_get_kernel_usec((uint64_t*)arg0);
		return;
	case SYS_GET_KERNEL_TIC:
		ctx->gpr[0] = sys_get_kernel_tic();
		return;
		*/
	case SYS_GET_PROCS: 
		ctx->gpr[0] = (int32_t)get_procs((int32_t*)arg0);
		return;
	case SYS_PROC_SET_ENV: 
		ctx->gpr[0] = proc_set_env((const char*)arg0, (const char*)arg1);
		return;
	case SYS_PROC_GET_ENV: 
		ctx->gpr[0] = sys_get_env((const char*)arg0, (char*)arg1, arg2);
		return;
	case SYS_PROC_GET_ENV_NAME: 
		ctx->gpr[0] = sys_get_env_name(arg0, (char*)arg1, arg2);
		return;
	case SYS_PROC_GET_ENV_VALUE: 
		ctx->gpr[0] = sys_get_env_value(arg0, (char*)arg1, arg2);
		return;
	case SYS_PROC_SHM_ALLOC:
		ctx->gpr[0] = sys_shm_alloc(arg0, arg1);
		return;
	case SYS_PROC_SHM_MAP:
		ctx->gpr[0] = (int32_t)sys_shm_map(arg0);
		return;
	case SYS_PROC_SHM_UNMAP:
		ctx->gpr[0] = sys_shm_unmap(arg0);
		return;
	case SYS_PROC_SHM_REF:
		ctx->gpr[0] = sys_shm_ref(arg0);
		return;
	case SYS_THREAD:
		ctx->gpr[0] = sys_thread(ctx, (uint32_t)arg0, (uint32_t)arg1, arg2);
		return;
	case SYS_KPRINT:
		sys_kprint((const char*)arg0, arg1, (bool)arg2);
		return;
	case SYS_MMIO_MAP:
		ctx->gpr[0] = sys_mmio_map();
		return;
	case SYS_FRAMEBUFFER_MAP:
		ctx->gpr[0] = sys_framebuffer_map((fbinfo_t*)arg0);
		return;
	case SYS_PROC_CRITICAL_ENTER:
		sys_proc_critical_enter();
		return;
	case SYS_PROC_CRITICAL_QUIT:
		sys_proc_critical_quit();
		return;
	case SYS_IPC_SETUP:
		ctx->gpr[0] = sys_ipc_setup(ctx, arg0, arg1, (bool)arg2);
		return;
	case SYS_IPC_CALL:
		sys_ipc_call(ctx, arg0, arg1, (proto_t*)arg2);
		return;
	case SYS_IPC_GET_RETURN:
		sys_ipc_get_return(ctx, arg0, (proto_t*)arg1);
		return;
	case SYS_IPC_SET_RETURN:
		sys_ipc_set_return((proto_t*)arg0);
		return;
	case SYS_IPC_END:
		sys_ipc_end(ctx);
		return;
	case SYS_IPC_GET_ARG:
		ctx->gpr[0] = sys_ipc_get_arg();
		return;
	case SYS_PROC_PING:
		ctx->gpr[0] = sys_proc_ping(arg0);
		return;
	case SYS_PROC_READY_PING:
		sys_proc_ready_ping();
		return;
	case SYS_GET_KEVENT:
		sys_get_kevent(ctx);
		return;
	case SYS_PROC_WAKEUP:
		sys_proc_wakeup(arg0);
		return;
	case SYS_PROC_BLOCK:
		sys_proc_block(ctx, arg0, arg1);
		return;
	case SYS_CORE_READY:
		sys_core_ready();
		return;
	}
	printf("pid:%d, code(%d) error!\n", _current_proc->pid, code);
}

