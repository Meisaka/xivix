#ifndef PS2_HAI
#define PS2_HAI

#include "ktypes.hpp"

namespace hw {

enum class PS2ST : uint32_t {
	INIT,
	INIT_KB,
	ECHO,
	LOST,
	IDLE,
};

class PS2 {
private:
	uint32_t kmd_q[16];
	uint32_t kmd_l;
	uint32_t nextcheck;
	uint32_t err_c;
	static void irq1_signal();
	static void irq12_signal();
	void irq1_handle();
	uint8_t ps2_get_read(bool force=false);
	void push_keycode(uint16_t v);
public:
	uint32_t err_n;
	volatile uint8_t icode;
	volatile uint8_t istatus;
	volatile uint32_t interupted;
	PS2ST cstatus;
	uint16_t keycode[12];

	static PS2 dev;
	PS2();
	~PS2();

	void init(); // setup controller
	bool waiting(); // wants handle called
	void handle(); // does stuff
	static void system_reset(); // reset the whole system

	void init_kbd(); // start a keyboard
	uint32_t port_query();
	void port_send(uint8_t c, uint32_t port);
};

}
#endif

