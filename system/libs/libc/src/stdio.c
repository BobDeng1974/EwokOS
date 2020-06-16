#include <stdio.h>
#include <vprintf.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mstr.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#include <fcntl.h>

static void outc(char c, void* p) {
	str_t* buf = (str_t*)p;
	str_addc(buf, c);
}

void dprintf(int fd, const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	str_t* buf = str_new("");
	v_printf(outc, buf, format, ap);
	va_end(ap);
	write(fd, buf->cstr, buf->len);
	str_free(buf);
}

void printf(const char *format, ...) {
	va_list ap;
	str_t* buf = str_new("");
	va_start(ap, format);
	v_printf(outc, buf, format, ap);
	va_end(ap);
	write(1, buf->cstr, buf->len);
	str_free(buf);
}

void kprintf(bool tty_only, const char *format, ...) {
	va_list ap;
	str_t* buf = str_new("");
	va_start(ap, format);
	v_printf(outc, buf, format, ap);
	va_end(ap);
	syscall3(SYS_KPRINT, (int32_t)buf->cstr, (int32_t)buf->len, (int32_t)tty_only);
	str_free(buf);
}

int getch(void) {
	while(1) {
		char c;
		int i = read(0, &c, 1);
		if(i == 1) {
			return c;
		}
		if(i <= 0 && errno != EAGAIN)
			break;
	}
	return 0;
}

void putch(int c) {
	write(1, &c, 1);
}

int puts(const char *s) {
	int ret = strlen(s);
	return write(1, s, ret);
}

FILE* fopen(const char* fname, const char* mode) {
	FILE *fp = (FILE*)malloc(sizeof(FILE));
	if(fp == NULL)
		return NULL;
	int oflags = 0;
	if(strcmp(mode, "r") == 0)
		oflags = O_RDONLY;
	else if(strcmp(mode, "r+") == 0)
		oflags = O_RDWR;
	else if(strcmp(mode, "w") == 0)
		oflags = O_WRONLY;
	else if(strcmp(mode, "w+") == 0)
		oflags = O_WRONLY | O_CREAT;

	fp->fd = open(fname, oflags);
	if(fp->fd < 0) {
		free(fp);
		return NULL;
	}
	fp->oflags = oflags;
	return fp;
}

void fclose(FILE* fp) {
	if(fp == NULL)
		return;
	close(fp->fd);
	free(fp);
}

uint32_t fread(void* ptr, uint32_t size, uint32_t nmemb, FILE* fp) {
	if(size == 0 || fp == NULL)
		return 0;

	int32_t rd = read(fp->fd, ptr, size*nmemb);
	if(rd < 0)
		rd = 0;
	return div_u32(rd, size);
}

uint32_t fwrite(const void* ptr, uint32_t size, uint32_t nmemb, FILE* fp) {
	if(size == 0)
		return 0;

	int32_t rd = write(fp->fd, ptr, size*nmemb);
	if(rd < 0)
		rd = 0;
	return div_u32(rd, size);
}

int fseek(FILE* fp, int offset, int whence) {
	if(fp == NULL)
		return -1;
	return lseek(fp->fd, offset, whence);
}

void rewind(FILE* fp) {
	fseek(fp, 0, SEEK_SET);
}

int ftell(FILE* fp) {
	return vfs_tell(fp->fd);
}

void fprintf(FILE* fp, const char *format, ...) {
	if(fp == NULL || ((fp->oflags & O_WRONLY) == 0 && (fp->oflags & O_RDWR) == 0))
		return;

	va_list ap;
	va_start(ap, format);
	str_t* buf = str_new("");
	v_printf(outc, buf, format, ap);
	va_end(ap);
	write(fp->fd, buf->cstr, buf->len);
	str_free(buf);
}

