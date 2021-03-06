#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/vfs.h>
#include <fcntl.h>

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

