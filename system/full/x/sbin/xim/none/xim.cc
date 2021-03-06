#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/vdevice.h>
#include <x/xclient.h>

class XIM {
 	int x_pid;
 	int keybFD;

	void input(char c) {
		xevent_t ev;
		ev.type = XEVT_IM;
		ev.value.im.value = c;

		proto_t in;
		PF->init(&in)->add(&in, &ev, sizeof(xevent_t));
		dev_cntl_by_pid(x_pid, X_DCNTL_INPUT, &in, NULL);
		PF->clear(&in);
	}

public:
	inline XIM() {
		x_pid = -1;
		keybFD = -1;
		while(true) {
			keybFD = open("/dev/keyb0", O_RDONLY);
			if(keybFD > 0)
				break;
			usleep(300000);
		}
	}

	inline ~XIM() {
		if(keybFD < 0)
			return;
		::close(keybFD);
	}

	void read(void) {
		if(x_pid < 0)
			x_pid = dev_get_pid("/dev/x");
		if(x_pid <= 0 || keybFD < 0)
			return;

		char v;
		int rd = ::read(keybFD, &v, 1);
		if(rd == 1) {
			input(v);
		}
	}
};

int main(int argc, char* argv[]) {
	(void)argc;
	(void)argv;

	XIM xim;
	while(true) {
		xim.read();
	}
	return 0;
}
