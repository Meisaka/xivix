#ifndef PS2_HAI
#define PS2_HAI

#include "ktypes.hpp"

namespace hw {

enum class PS2ST : uint32_t {
	LOST,
	INIT,
	ECHO,
	IDLE,
	IDENT,
	IDENT1,
	IDENT2,
};
enum class PS2TYPE : uint32_t {
	QUERY,
	KEYBOARD,
	MOUSE3,
	MOUSE4,
	MOUSE5,
};

struct PS2Port {
	PS2ST status;
	PS2TYPE type;
	uint32_t nextcheck;
	uint32_t err_c;
};

class PS2 {
private:
	uint32_t kmd_q[16];
	uint32_t kmd_l;
	static void irq1_signal();
	static void irq12_signal();
	void irq1_handle();
	void irq12_handle();
	uint8_t ps2_get_read(bool force=false);
	void push_keycode(uint16_t v);
	static void send_data(uint8_t v);
	static void send_cmd(uint8_t v);
	bool cinit;
public:
	uint32_t err_n;
	volatile uint8_t icode[4];
	volatile uint8_t istatus[4];
	volatile uint32_t interupted;
	//PS2ST cstatus; // status of controller
	PS2Port port[4];
	uint16_t keycode[12];

	static PS2 dev;
	PS2();
	~PS2();

	void init(); // setup controller
	bool waiting(); // wants handle called
	void handle(); // does stuff
	static void system_reset(); // reset the whole system

	void init_kbd(uint32_t p); // start a keyboard
	uint32_t port_query(uint32_t p);
	void port_send(uint8_t c, uint32_t p);
};

}
#endif

