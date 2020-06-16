#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/vfs.h>
#include <sys/syscall.h>
#include <sys/vdevice.h>
#include <graph/graph.h>
#include <sys/shm.h>
#include <fbinfo.h>
#include <sys/critical.h>

typedef struct {
	void* data;
	uint32_t size;
	int32_t shm_id;
} fb_dma_t;

static fbinfo_t _fbinfo;

static int fb_write(int fd,
		int ufid,
		int from_pid, 
		fsinfo_t* info, 
		const void* buf,
		int size,
		int offset,
		void* p) {

	(void)fd;
	(void)ufid;
	(void)from_pid;
	(void)info;
	(void)offset;
	(void)p;

	int32_t sz = (_fbinfo.depth/8) * _fbinfo.width * _fbinfo.height;
	if(size < sz)
		return 0;
	
	if(_fbinfo.depth == 32) {
		critical_enter();
		memcpy((void*)_fbinfo.pointer, buf, size);
		critical_quit();
	}
	else if(_fbinfo.depth == 16) 
		graph_dup16((uint16_t*)_fbinfo.pointer, (uint32_t*)buf, _fbinfo.width, _fbinfo.height);
	return sz;
}	

static int fb_fcntl(int fd, 
		int ufid,
		int from_pid,
		fsinfo_t* info, 
		int cmd, 
		proto_t* in, 
		proto_t* out,
		void* p) {

	(void)fd;
	(void)ufid;
	(void)from_pid;
	(void)info;
	(void)in;
	(void)p;

	if(cmd == CNTL_INFO) {
		PF->addi(out, _fbinfo.width)->addi(out, _fbinfo.height);
	}
	return 0;
}

static int fb_flush(int fd, int from_pid, fsinfo_t* info, void* p) {
	(void)fd;
	(void)from_pid;
	(void)info;
	fb_dma_t* dma = (fb_dma_t*)p;

	uint32_t size = dma->size;
	uint32_t sz = (_fbinfo.depth/8) * _fbinfo.width * _fbinfo.height;
	if(size > sz)
		size = sz;

	if(_fbinfo.depth == 32) {
		critical_enter();
		memcpy((void*)_fbinfo.pointer, dma->data, size);
		critical_quit();
	}
	else if(_fbinfo.depth == 16) 
		graph_dup16((uint16_t*)_fbinfo.pointer, (uint32_t*)dma->data, _fbinfo.width, _fbinfo.height);
	return 0;
}

static int fb_dma(int fd, int ufid, int from_pid, fsinfo_t* info, int* size, void* p) {
	(void)fd;
	(void)ufid;
	(void)from_pid;
	(void)info;
	fb_dma_t* dma = (fb_dma_t*)p;
	*size = dma->size;
	return dma->shm_id;
}

int main(int argc, char** argv) {
	const char* mnt_name = argc > 1 ? argv[1]: "/dev/fb0";

	syscall1(SYS_FRAMEBUFFER_MAP, (int32_t)&_fbinfo);
	uint32_t sz = _fbinfo.width * _fbinfo.height * 4;

	fb_dma_t dma;
	dma.shm_id = shm_alloc(sz, 1);
	if(dma.shm_id <= 0)
		return -1;
	dma.size = sz;
	dma.data = shm_map(dma.shm_id);
	if(dma.data == NULL)
		return -1;

	vdevice_t dev;
	memset(&dev, 0, sizeof(vdevice_t));
	strcpy(dev.name, "framebuffer");
	dev.dma = fb_dma;
	dev.flush = fb_flush;
	dev.write = fb_write;
	dev.fcntl = fb_fcntl;

	dev.extra_data = &dma;
	device_run(&dev, mnt_name, FS_TYPE_CHAR);

	shm_unmap(dma.shm_id);
	return 0;
}
