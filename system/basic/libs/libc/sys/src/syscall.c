#include <sys/syscall.h>
#include <sys/ipc.h>

#ifdef __cplusplus
extern "C" {
#endif


inline int32_t syscall3_raw(int32_t code, int32_t arg0, int32_t arg1, int32_t arg2) {
	volatile int32_t r;
  __asm__ volatile(
			"stmdb sp!, {lr}\n"
			"mrs   r0,  cpsr\n"
			"stmdb sp!, {r0}\n"
			"mov r0, %1 \n"
			"mov r1, %2 \n"
			"mov r2, %3 \n"
			"mov r3, %4 \n"
			"swi #0     \n"
			"mov %0, r0 \n" // assign r  = r0
			"ldmia sp!, {r1}\n"
			"ldmia sp!, {lr}\n"
			: "=r" (r) 
			: "r" (code), "r" (arg0), "r" (arg1), "r" (arg2)
			: "r0", "r1", "r2", "r3" );
	return r;
}

inline int32_t syscall3(int32_t code, int32_t arg0, int32_t arg1, int32_t arg2) {
	int32_t res = syscall3_raw(code, arg0, arg1, arg2);
	return res;
}

inline int32_t syscall2(int32_t code, int32_t arg0, int32_t arg1) {
	int32_t res = syscall3_raw(code, arg0, arg1, 0);
	return res;
}

inline int32_t syscall1(int32_t code, int32_t arg0) {
	int32_t res = syscall3_raw(code, arg0, 0, 0);
	return res;
}

inline int32_t syscall0(int32_t code) {
	int32_t res = syscall3_raw(code, 0, 0, 0);
	return res;
}

#ifdef __cplusplus
}
#endif

