#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/ipc.h>
#include <sys/vfs.h>
#include <stddef.h>
#include <string.h>

int open(const char* fname, int oflag) {
	int fd = -1;
	fsinfo_t info;

	if(vfs_get(fname, &info) != 0) {
		if((oflag & O_CREAT) != 0) {
			if(vfs_create(fname, &info, FS_TYPE_FILE) != 0)
				return -1;
		}
		else  {
			return -1;	
		}	
	}

	fd = vfs_open(&info, oflag);
	if(fd < 0)
		return -1;
	
	proto_t in, out;
	PF->init(&out, NULL, 0);

	PF->init(&in, NULL, 0)->
		addi(&in, fd)->
		add(&in, &info, sizeof(fsinfo_t))->
		addi(&in, oflag);

	if(ipc_call(info.mount_pid, FS_CMD_OPEN, &in, &out) != 0 ||
			proto_read_int(&out) != 0) {
		vfs_close(fd);
		fd = -1;
	}

	PF->clear(&in);
	PF->clear(&out);
	return fd;
}


void close(int fd) {
	fsinfo_t info;
	if(vfs_get_by_fd(fd, &info) != 0)
		return;

	proto_t in;
	PF->init(&in, NULL, 0)->
		addi(&in, fd)->
		addi(&in, -1)->
		add(&in, &info, sizeof(fsinfo_t));

	ipc_call(info.mount_pid, FS_CMD_CLOSE, &in, NULL);
	PF->clear(&in);
	vfs_close(fd);
}

int dma(int fd, int* size) {
	fsinfo_t info;
	if(vfs_get_by_fd(fd, &info) != 0)
		return -1;
	
	proto_t in, out;
	PF->init(&out, NULL, 0);

	PF->init(&in, NULL, 0)->
		addi(&in, fd)->
		add(&in, &info, sizeof(fsinfo_t));

	int shm_id = -1;
	if(ipc_call(info.mount_pid, FS_CMD_DMA, &in, &out) == 0) {
		shm_id = proto_read_int(&out);
		if(size != NULL)
			*size = proto_read_int(&out);
	}
	PF->clear(&in);
	PF->clear(&out);
	return shm_id;
}

void flush(int fd) {
	fsinfo_t info;
	if(vfs_get_by_fd(fd, &info) != 0)
		return;
	
	proto_t in, out;
	PF->init(&out, NULL, 0);

	PF->init(&in, NULL, 0)->
		addi(&in, fd)->
		add(&in, &info, sizeof(fsinfo_t));
	ipc_call(info.mount_pid, FS_CMD_FLUSH, &in, &out);
	PF->clear(&in);
	PF->clear(&out);
}

