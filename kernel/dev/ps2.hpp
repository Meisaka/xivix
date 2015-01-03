#ifndef PS2_HAI
#define PS2_HAI

#include "ktypes.hpp"
#include "hwtypes.hpp"

namespace hw {

enum class PS2ST : uint32_t {
	LOST,
	INIT,
	ECHO,
	IDLE,
	BUSY,
	IDENT,
	IDENTE,
	IDENT1,
	IDENTE1,
	IDENT2,
	DISABLE,
};
enum class PS2TYPE : uint32_t {
	QUERY,
	KEYBOARD,
	MOUSE3,
	MOUSE4,
	MOUSE5,
};

class Keyboard : public Hardware, public MultiPortClient {
private:
	void key_down();
	void key_up();
	void init_proc();
	uint8_t kbd_cmd(uint8_t v);
	void kbd_data(uint8_t v);
	uint32_t keybuf[16];
	uint32_t keyev;
	uint32_t istate;
public:
	uint32_t state;
	uint32_t keycode;
	uint32_t lastkey1;
	uint32_t lastkey2;
	uint8_t lastscan;

	uint32_t mods;

	void push_key(uint32_t);
	uint32_t pop_key();
	bool has_key();

	Keyboard();
	~Keyboard();

	bool init() override;
	void remove() override;
	void port_data(uint8_t) override;
};

struct PS2Port {
	PS2ST status;
	PS2TYPE type;
	uint32_t nextcheck;
	uint32_t err_c;
	MultiPortClient *cl_ptr;
};

class PS2 : public Hardware, public MultiPort, public MultiPortService {
private:
	uint32_t kmd_q[16];
	uint32_t kmd_l;
	static void irq1_signal();
	static void irq12_signal();
	void irq1_handle();
	void irq12_handle();
	uint8_t ps2_get_read();
	void push_keycode(uint16_t v);
	static void send_data(uint8_t v);
	static void send_adata(uint8_t v);
	static void send_cmd(uint8_t v);
	void signal_loss(uint32_t u);
	bool cinit;
public:
	uint32_t err_n;
	volatile uint8_t icode[4];
	volatile uint8_t istatus[4];
	volatile uint32_t interupted;
	Keyboard *kb_drv;
	PS2Port port[4];
	uint16_t keycode[12];
	uint16_t keycount;

	static PS2 dev;
	PS2();
	~PS2();

	bool init() override; // setup controller
	void remove() override {}
	bool waiting(); // wants handle called
	void handle(); // does stuff
	static void system_reset(); // reset the whole system

	void init_kbd(uint32_t p); // start a keyboard
	void add_kbd(Keyboard*); // add a keyboard driver
	void client_send(uint32_t v, uint32_t u) override {
		port_send(cast<uint8_t>(v), u);
	}
	uint32_t client_req(uint32_t u) override {
		return port_query(u);
	}
	void add_client(MultiPortClient*, uint32_t u) override;
	void remove_client(MultiPortClient*, uint32_t u) override;

	void port_enable(uint32_t p, bool ena);
	uint32_t port_query(uint32_t p) override;
	void port_send(uint8_t c, uint32_t p) override;
	void port_senda(uint8_t c, uint32_t p);
};

}
#endif

