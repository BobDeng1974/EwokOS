#include <mm/mmu.h>
#include <dev/actled.h>
#include <bcm283x/gpio.h>

void actled(bool on) {
	uint32_t ra;
	ra = get32(GPIO_FSEL4);
	ra &= ~(7<<21);
	ra |= 1<<21;
	put32(GPIO_FSEL4, ra);

	if(!on) 	
		put32(GPIO_CLR1, 1<<(47-32));
	else
		put32(GPIO_SET1, 1<<(47-32));
}

