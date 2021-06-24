/* ***
 * e1000.cpp - e1000 / Intel 825xx NIC driver
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */

#include "e1000.hpp"
#include "ktext.hpp"
#include "kio.hpp"
#include "memory.hpp"
#include "interrupt.hpp"

namespace hw {

uint16_t e1000::readeeprom(uint16_t epma) {
	vio[0x14] = 1u | (epma << 2);
	while(!(vio[0x14] & 0x2)) { }
	return (uint16_t)(vio[0x14] >> 16);
}

#pragma pack(push, 1)
union HWR_CTRL {
	uint32_t value;
	struct {
		uint32_t fd : 1;
		uint32_t rsvr1 : 1;
		uint32_t gio_dis : 1;
		uint32_t lrst : 1;
		uint32_t rsv4 : 2;
		uint32_t slu : 1;
		uint32_t rsv7 : 1;
		uint32_t speed : 2;
		uint32_t rsv10 : 1;
		uint32_t frcspd : 1;
		uint32_t frcdplx : 1;
		uint32_t rsv13 : 2;
		uint32_t rsv15 : 1;
		uint32_t rsv16 : 1;
		uint32_t rsv17 : 1;
		uint32_t sdp0_data : 1;
		uint32_t sdp1_data : 1;
		uint32_t advd3wuc : 1;
		uint32_t rsv21 : 1;
		uint32_t sdp0_iodir : 1;
		uint32_t sdp1_iodir : 1;
		uint32_t rsv24 : 2;
		uint32_t rst : 1;
		uint32_t rfce : 1;
		uint32_t tfce : 1;
		uint32_t rsv29 : 1;
		uint32_t vme : 1;
		uint32_t phy_rst : 1;
	};
	void status() {
		xiv::printf("e1000: CTRL:%08x", value);
		if(slu) xiv::print(" SLU");
		if(lrst) xiv::print(" LRST");
		if(gio_dis) xiv::print(" GIOMD");
		xiv::printf(" SPEED:%x", 0x10 << (speed * 4));
		xiv::print(fd? " Full":" Half");
		if(frcspd) {
			xiv::print(" FRCSPD");
		}
		if(frcdplx) {
			xiv::print(" FRCDPLX");
		}
		if(rfce) xiv::print(" RFCE");
		if(tfce) xiv::print(" TFCE");
		if(vme) xiv::print(" VME");
		if(phy_rst) xiv::print(" PHYRST");
		xiv::putc(10);
	}
};
union HWR_STATUS {
	uint32_t value;
	struct {
		uint32_t fd : 1;
		uint32_t lu : 1;
		uint32_t lanid : 2;
		uint32_t txoff : 1;
		uint32_t tbimode : 1;
		uint32_t speed : 2;
		uint32_t asdv : 2;
		uint32_t phyra : 1;
		uint32_t rsv0 : 1;
		uint32_t rsv1 : 7;
		uint32_t giomes : 1;
		uint32_t rsv2 : 12;
	};
	void status() {
		xiv::printf("e1000: STATUS:%08x", value);
		if(lu) xiv::print(" LU");
		xiv::printf(" LANID:%x", lanid);
		if(txoff) xiv::print(" TXOFF");
		if(tbimode) xiv::print(" TBIMODE");
		if(asdv) xiv::print(" ASDV");
		if(phyra) xiv::print(" PHYRA");
		xiv::printf(" SPEED:%x", 0x10 << (speed * 4));
		xiv::print(fd? " Full":" Half");
		if(giomes) xiv::print(" GIOMES");
		xiv::putc(10);
	}
};
union HWR_IMS {
	uint32_t value;
	struct {
		uint32_t txdw : 1;
		uint32_t txqe : 1;
		uint32_t lsc : 1;
		uint32_t rxseq : 1;
		uint32_t rxdmt0 : 1;
		uint32_t rsv0 : 1;
		uint32_t rxo : 1;
		uint32_t rxt0 : 1;
		uint32_t rsv1 : 1;
		uint32_t mdac : 1;
		uint32_t rxcfg : 1;
		uint32_t rsv2 : 1;
		uint32_t phyint : 1;
		uint32_t gpi : 2;
		uint32_t txd_low : 1;
		uint32_t srpd : 1;
		uint32_t rsv3 : 15;
	};
};
enum RCTL : uint32_t {
	RCTL_Undefined = 0,
	RCTL_EN = 1<<1,
	RCTL_SBP = 1<<2,
	RCTL_UPE = 1<<3,
	RCTL_MPE = 1<<4,
	RCTL_LPE = 1<<5,
	RCTL_LO_normal = 0<<6,
	RCTL_LO_mac = 1<<6,
	RCTL_RDMTS_1_2 = 0<<8,
	RCTL_RDMTS_1_4 = 1<<8,
	RCTL_RDMTS_1_8 = 2<<8,
	RCTL_DTYP_legacy = 0<<10,
	RCTL_DTYP_split =  1<<10,
	RCTL_MO_36 = 0<<12,
	RCTL_MO_35 = 1<<12,
	RCTL_MO_34 = 2<<12,
	RCTL_MO_32 = 3<<12,
	RCTL_BAM = 1<<15,
	RCTL_BSIZE_2048 = 0<<16,
	RCTL_BSIZE_1024 = 1<<16,
	RCTL_BSIZE_512 = 2<<16,
	RCTL_BSIZE_256 = 3<<16,
	RCTL_BSIZE_16384 = (1<<25) | (1<<16),
	RCTL_BSIZE_8192 = (1<<25) | (2<<16),
	RCTL_BSIZE_4096 = (1<<25) | (3<<16),
	RCTL_VFE = 1<<18,
	RCTL_CFIEN = 1<<19,
	RCTL_CFI = 1<<20,
	RCTL_DPF = 1<<22,
	RCTL_PMCF = 1<<23,
	RCTL_SECRC = 1<<26,
};
union HWR_RCTL {
	uint32_t value;
	struct {
		uint32_t rsv0 : 1;
		uint32_t en : 1;
		uint32_t sbp : 1;
		uint32_t upe : 1;
		uint32_t mpe : 1;
		uint32_t lpe : 1;
		uint32_t lo : 2;
		uint32_t rdmts : 2;
		uint32_t dtyp : 2;
		uint32_t mo : 2;
		uint32_t rsv14 : 1;
		uint32_t bam : 1;
		uint32_t bsize : 2;
		uint32_t vfe : 1;
		uint32_t cfien : 1;
		uint32_t cfi : 1;
		uint32_t rsv21 : 1;
		uint32_t dpf : 1;
		uint32_t pmcf : 1;
		uint32_t rsv24 : 1;
		uint32_t bsex : 1;
		uint32_t secrc : 1;
		uint32_t flxbuf : 4;
		uint32_t rsv31 : 1;
	};
	void status() {
		const char *s_dtyp[] = {"LEG","PKSPL","RSV2","RSV3"};
		xiv::printf("e1000: RCTL:%08x%s%s%s%s%s LO=%01d RDMTS=%01d DTYP=%s MO=%d%s%s BSIZE=%d%s%s%s%s%s%s%s%s FLXBUF=%dk%s\n", value,
		en ? " EN" : "",
		sbp ? " STBAD":"",
		upe ? " UPMSC":"",
		mpe ? " MPMSC":"",
		lpe ? " LONGP":"",
		lo,
		rdmts,
		s_dtyp[dtyp],
		36 - mo,
		rsv14 ? " RSV14":"",
		bam ? " BRDA":"",
		(2048 >> bsize) << (bsex ? 8 : 0),
		vfe ? " VFE":"",
		cfien ? " CFIEN":"",
		cfi ? " CFI":"",
		rsv21 ? " RSV21":"",
		dpf ? " DSPAU":"",
		pmcf ? " PAMAC":"",
		rsv24 ? " RSV24":"",
		secrc ? " NOCRC":"",
		flxbuf,
		rsv31 ? " RSV31":"");
	}
};
union HWR_TCTL {
	uint32_t value;
	struct {
		uint32_t rsv0 : 1;
		uint32_t en : 1;
		uint32_t rsv1 : 1;
		uint32_t psp : 1;
		uint32_t ct : 8;
		uint32_t cold : 10;
		uint32_t swxoff : 1;
		uint32_t rsv2 : 1;
		uint32_t rtlc : 1;
		uint32_t nrtu : 1;
		uint32_t rsv3 : 6;
	};
	void status() {
		xiv::printf("e1000: TCTL:%08x", value);
		if(en) xiv::print(" EN");
		if(psp) xiv::print(" PSP");
		xiv::printf(" CT=%d", ct);
		xiv::printf(" COLD=%d", cold);
		if(swxoff) xiv::print(" SWXOFF");
		if(rtlc) xiv::print(" RTLC");
		if(nrtu) xiv::print(" NRTU");
		xiv::putc(10);
	}
};
struct RECV_DESC {
	uint64_t address;
	uint16_t length;
	uint16_t checksum;
	uint8_t status;
	uint8_t errors;
	uint16_t vlan;
} __attribute__((packed));
struct TRMTDESC_EXCONTEXT { // TCP/IP context descriptor (non-data config)
	uint8_t ipcss;
	uint8_t ipcso;
	uint16_t ipcse;
	uint8_t tucss;
	uint8_t tucso;
	uint16_t tucse;
	uint32_t len : 20;
	uint32_t dtype : 4;
	uint32_t tucmd : 8;
	uint8_t status;
	uint8_t hdrlen;
	uint16_t mss;
};
enum TRMTDESC_DCMD : uint8_t {
	TDC_EOP = 1, // end of packet
	TDC_IFCS = 1<<1, // insert IFCS (adds the FCS/CRC) we usually want this
	TDC_TSE = 1<<2, // TCP segmentation enable
	TDC_RS = 1<<3, // report status
	TDC_DEXT = 1<<5, // non-legacy descriptors need this
	TDC_VLE = 1<<6, // enable VLANs (*_INFO struct)
	TDC_IDE = 1<<7, // interrupt delay enable, for use with RS
};
enum TRMTDESC_STATUS : uint8_t {
	TDS_DD = 1, // written back (if RS was set)
};

struct TRMTDESC_EXDATA { // TCP/IP data descriptor
	uint32_t len : 20;
	uint32_t dtype : 4; // set dtype = 1
	uint32_t dcmd : 8; // set DEXT bit for this struct
	uint8_t status;
	uint8_t popts;
	uint16_t vlan;
} __attribute__((packed));
union TRMT_UN {
	uint64_t raw;
	TRMTDESC_EXDATA exdata;
	TRMTDESC_EXCONTEXT excontext;
};
struct TRMT_DESC {
	volatile uint64_t address;
	volatile uint64_t info;
};
struct e1000::DESC_PAGE {
	RECV_DESC recv[128];
	TRMT_DESC trmt[128];
	Ref<net::Packet> recv_slot[128];
	Ref<net::Packet> trmt_slot[128];
};
#pragma pack(pop)

void e1000_intcall(void * u, uint32_t, ixintrctx *) {
	if(u) {
		((e1000*)u)->handle_int();
	}
}

uint32_t e1000::handle_int() {
	uint32_t its = vio[0xc0];
	_ixa_or(&lastint, its);
	uint32_t itt = _ivix_int_n;
	junk_register += its + _ivix_int_n - last_itime;
	last_itime = itt;
	junk_register += vio[0x2810];
	junk_register += vio[0x3810];
	return 0;
}

e1000::e1000(pci::PCIBlock &pcib) {
	xiv::print("e1000: add device\n");
	xiv::printf("e1000: base: %x / %x\n", pcib.bar[0], pcib.barsz[0]);
	xiv::printf("e1000: intr: %x / %x\n", pcib.info.int_line, pcib.info.int_pin);
	lastint = 0;
	pcib.writecommand(0x117); // enable mastering on PCI
	junk_register = 0;
	last_junk = 0;
	// setup the ring buffer pages
	uint64_t piobase = (pcib.bar[0] & 0xfffffff0);
	void * vmbase = (void*)(pcib.bar[0] & 0xfffffff0);
	vio.base = (volatile uint8_t *)vmbase;
	mem::vmm_request(pcib.barsz[0], vmbase, piobase, mem::RQ_HINT | mem::RQ_RW | mem::RQ_RCD | mem::RQ_WCD);
	constexpr size_t descpage_size = 2 * mem::PAGE_SIZE;
	descpage = (DESC_PAGE*)mem::vmm_request(descpage_size, nullptr, 0, mem::RQ_RW | mem::RQ_ALLOC | mem::RQ_RCD | mem::RQ_WCD);
	size_t *vp = (size_t*)descpage;
	for(size_t n = 0; n < (descpage_size / sizeof(size_t)); n++) {
		vp[n] = 0;
	}
	ivix_interrupt[pcib.info.int_line] = { .entry = e1000_intcall, .rlocal = this };
}

e1000::~e1000() {
}
bool e1000::init() {
	xiv::print("e1000: init...\n");
	// Device Control
	HWR_CTRL r_ctrl = { .value = 0 };
	HWR_IMS ims = { .value = 0 };
	vio[0xd8] = 0xffffffff; // all interrupts off
	vio[0x100] = 0;
	vio[0x400] = 0;
	r_ctrl.gio_dis = 1;
	uint32_t nani = 0;
	while(vio[8] & (1 << 19)) { _ix_halt(); if(++nani==100) break; } // GIO disable for PCIe
	r_ctrl.rst = 1;
	r_ctrl.slu = 1;
	r_ctrl.lrst = 1;
	r_ctrl.phy_rst = 1;
	vio[0] = r_ctrl.value; // reset everything
	while(vio[0] & ((1 << 26))) { // await RST to clear
		nani++;
		if(nani>10000) {
			_ix_halt();
			if(nani > 11000) break;
		}
	}
	vio[0xd8] = 0xffffffff; // all interrupts off
	uint32_t floof[4];
	for(size_t i = 0; i < 73;) {
		floof[i & 3] = vio[0x4000 + (i * 4)]; // dig for gold
		i++;
		if((i & 3) == 0) Crypto::Mix(floof, 4);
	}
	Crypto::Mix(floof, 4);
	xiv::printf("e1000: RST %d\n", nani);
	HWR_STATUS dsr = { .value = vio[8] };
	if(dsr.tbimode) {
		//vio[0x18] = (3 << 22) | (1 << 28); // clear CTRL_EXT in case of weirdness
	}
	r_ctrl.value = 0; // manually clear all the bits... I guess.
	r_ctrl.fd = 1;
	r_ctrl.slu = 1; // link up
	r_ctrl.lrst = 1; // 82573x wants this as 1, a write to 0 triggers AN
	vio[0] = r_ctrl.value;
	xiv::print("e1000: wait link\n");
	size_t lu_wait = 50;
	while((vio[8] & (1 << 1)) == 0) { // wait link up, TODO not forever!!!1!
		_ix_halt();
		if(!(--lu_wait)) break;
	}
	dsr.value = vio[8];
	dsr.status();
	r_ctrl.value = vio[0];
	r_ctrl.status();
	// TODO items for general/global reset:
	// MAC mode from LINK_MODE in Device Status register
	// FD/Speed set per interface (or auto-negotiate)
	// BEM (whatever that is)
	// maybe do something with the LRST
	// CTRL ILOS to 0 on 8257[1/2]E[B/I] ???
	
	// Flow control registers
	vio[0x028] = 0x00c28001;
	vio[0x02c] = 0x0100;
	vio[0x030] = 0x8808;
	vio[0x038] = 0x8100; // VLAN ethertype

	vio[0x170] = 0xffff; // FCTTV recommended (so whatever...)
	vio[0x5808] = 0; // WUFC clear (we don't need it's features)
	// TODO: power up/reset/link up, items:
	// TARC0: 26:23, 21, 20, 6:0
	// TARC1: 28, 26:24, 22, 6:0
	// TODO Power management maybe?:
	// ofs14 page c1 = 9 or 0x11
	// ofs10 page c1 = 0x180(gig) or 0x980(fast)
	// following items are recommended for 63[12]xESB only
	//vio[0x5f04] = 0; // TODO 63[12]xESB only
	// TODO KUMCTRLSTA: 10=0(gig) or 4(fast)
	//vio[0x034] = (10<< 16) | (0); // 0=gig, 4=10/100
	//vio[0x034] = (0 << 16) | 0x808;
	//vio[0x034] = (2 << 16) | 0x510;

	// MAC address
	// From EEPROM:
	//for(int x = 0; x < 3; x++) ((uint16_t*)macaddr)[x] = readeeprom(x);
	// From Register (defaulted after reset):
	((uint32_t*)macaddr)[0] = vio[0x5400];
	((uint16_t*)macaddr)[2] = (uint16_t)vio[0x5404];
	xiv::printf("e1000: MAC: %x", macaddr[0]);
	for(int x = 1; x < 6; x++) xiv::printf(":%x", macaddr[x]);
	xiv::putc(10);

	// disable receive/transmit to setup rings
	// Recieve Control
	// select receive types
	vio[0x100] = RCTL_BSIZE_2048|RCTL_BAM|RCTL_MPE/*|RCTL_UPE*/|RCTL_SBP;
	vio[0x2160] = 0; // FCRTL
	vio[0x2170] = 16; // split receive settings (2048 in a single buffer)
	// TODO we don't use split receive, test without this
	uint64_t rdbuf = mem::translate_page((uint32_t)descpage->recv);
	vio[0x2800] = (uint32_t)rdbuf; // setup the ring buffer
	vio[0x2804] = (uint32_t)(rdbuf >> 32);
	rxq.limit = sizeof(DESC_PAGE::recv);
	vio[0x2808] = rxq.limit;
	rxq.limit >>= 4;
	rxq.tail = 0;
	rxq.head = 0;
	vio[0x2818] = 0; // R tail
	vio[0x2810] = 0; // R head
	vio[0x2828] = (0) | (0 << 8) | ((1) << 16) | (1 << 24);
	vio[0x282c] = 0; // disable RADV
	vio[0x2c00] = 0;
	vio[0x5008] = 0<<15;
	vio[0x100] = RCTL_BSIZE_2048|RCTL_BAM|RCTL_MPE/*|RCTL_UPE*/|RCTL_SBP|RCTL_EN;
	// Transmit Control
	HWR_TCTL tctl;
	tctl.value = 0;
	// spec recommended TCTL settings:
	tctl.ct = 15;
	tctl.cold = 0x40;
	tctl.rtlc = 1;
	tctl.psp = 1; // we use this
	tctl.en = 0; // disable during setup
	vio[0x400] = tctl.value; // make sure transmit is disabled.
	uint64_t txbuf = mem::translate_page((uint32_t)descpage->trmt);
	vio[0x3800] = (uint32_t)txbuf;
	vio[0x3804] = (uint32_t)(txbuf>>32);
	txq.limit = sizeof(DESC_PAGE::trmt);
	vio[0x3808] = txq.limit;
	txq.limit >>= 4;
	txq.tail = 0;
	txq.head = 0;
	vio[0x3818] = 0;
	vio[0x3810] = 0; // setup the descriptor ring buffer
	vio[0x404] = 0x40 << 10; // set TCTL_EXT as per spec recommended
	// maybe set TIPG?! (TIPG.IPGT[9:0] depends on link speed 10/100 should be 9)
	vio[0x410] = (9) | ((8) << 10) | ((7) << 20);
	// setup TXDCTL maybe? bit 22 is to be set on some devices?!
	vio[0x3828] = (1 << 8) | (1 << 16) | (1 << 24) | (1 << 22);
	vio[0x382c] = 103; // Absolute interrupt delay (n * 1.024 microsecs)
	vio[0x3840] = 1/*COUNT*/ | (1/*ENABLE*/ << 10);
	tctl.en = 1;
	vio[0x400] = tctl.value; // enable transmit

	// enable interrupts
	ims.value = 0x0;
	ims.txdw = 1;
	ims.txqe = 1;
	ims.lsc = 1;
	ims.rxseq = 1;
	ims.rxdmt0 = 1;
	ims.rxo = 1;
	ims.rxt0 = 1;
	ims.mdac = 1;
	ims.txd_low = 1;
	vio[0xd0] = ims.value;

	//r_ctrl.value = vio[0];
	//r_ctrl.slu = 1; // link up
	//r_ctrl.lrst = 1;
	//r_ctrl.gio_dis = 0;
	//vio[0] = r_ctrl.value;
	for(size_t i = 0; i < 73;) { // clear all the stats registers
		floof[i & 3] = vio[0x4000 + (i * 4)]; // dig for gold
		i++;
		if((i & 3) == 0) Crypto::Mix(floof, 4);
	}
	return true;
}
void e1000::debug_junk() {
	uint32_t iv = junk_register - last_junk;
	last_junk = junk_register;
	if(!iv) return;
	Crypto::Mix(&iv, 1);
}

void e1000::debug_cmd(void *cmdv, uint32_t cmdl) {
	(void)cmdv;
	HWR_CTRL r_ctrl = { .value = vio[0] };
	HWR_STATUS dsr = { .value = vio[8] };
	r_ctrl.status();
	dsr.status();
	xiv::printf("e1000: PBA: %08x\n", vio[0x1000]);
	xiv::printf("e1000: LED: %08x\n", vio[0xe00]);
	xiv::printf("e1000: CTRLEXT: %x\n", vio[0x18]);
	xiv::printf("e1000: SFSYNC: %x\n", vio[0x5b5c]);
	xiv::printf("e1000: TXCW: %x\n", vio[0x178]);
	xiv::printf("e1000: RXCW: %x\n", vio[0x170]);
	HWR_IMS fint; fint.value = vio[0xd0];
	xiv::printf("e1000: IMS: %x %s%s%s%s%s%s%s%s\n", fint,
		fint.txdw ? " TXDW" : "",
		fint.txqe ? " TXQE" : "",
		fint.lsc ? " LSC" : "",
		fint.rxseq ? " RXSEQ" : "",
		fint.rxdmt0 ? " RXDMT0" : "",
		fint.rxo ? " RXO" : "",
		fint.rxt0 ? " RXT0" : "",
		fint.txd_low ? " TXD-LOW" : ""
	);
	HWR_TCTL tctl;
	tctl.value = vio[0x400];
	tctl.status();
	uint64_t trbuf = mem::translate_page((uint32_t)descpage->trmt);
	xiv::printf("e1000: TXRING: %012lx <> %04x%08x %016lx\n", trbuf, vio[0x3804], vio[0x3800]);
	xiv::printf("e1000: TDLEN : %08x\n", vio[0x3808]);
	xiv::printf("e1000: TXHEAD: %08x <> %08x\n", txq.head, vio[0x3810]);
	xiv::printf("e1000: TXTAIL: %08x <> %08x\n", txq.tail, vio[0x3818]);
	xiv::printf("e1000: TXDCTL: %08x\n", vio[0x3828]);
	xiv::printf("e1000: TXDCTL1 %08x\n", vio[0x3928]);
	xiv::printf("e1000: TARC0 : %08x\n", vio[0x3840]);
	xiv::printf("e1000: TIDV  : %08x\n", vio[0x3820]);
	xiv::printf("e1000: TADV  : %08x\n", vio[0x382c]);
	HWR_RCTL rctl;
	rctl.value = vio[0x100];
	rctl.status();
//	xiv::printf("e1000: RCTL: %08x\n", vio[0x100]);
	xiv::printf("e1000: RXCW: %08x\n", vio[0x180]);
	uint64_t rdbuf = mem::translate_page((uint32_t)descpage->recv);
	xiv::printf("e1000: RXRING: %012lx <> %04x%08x %016lx\n", rdbuf, vio[0x2804], vio[0x2800]);
	xiv::printf("e1000: RXHEAD: %08x <> %08x\n", rxq.head, vio[0x2810]);
	xiv::printf("e1000: RXTAIL: %08x <> %08x\n", rxq.tail, vio[0x2818]);
	xiv::printf("e1000: RDFH: > %08x > %08x\n", vio[0x2410], vio[0x2420]);
	xiv::printf("e1000: RDFT: > %08x > %08x\n", vio[0x2418], vio[0x2428]);
	xiv::printf("e1000: RXDCTL: %08x\n", vio[0x2828]);
	xiv::printf("e1000: s-RNBC: %08x\n", vio[0x40a0]);
	xiv::printf("e1000: s-TPR : %08x\n", vio[0x40d0]);
	xiv::printf("e1000: RFCTL : %08x\n", vio[0x5008]);
	if(cmdl == 1) {
		char cc = *(char*)cmdv;
		switch(cc) {
		case '!':
			r_ctrl.rst = 1;
			r_ctrl.lrst = 1;
			r_ctrl.phy_rst = 1;
			vio[0] = r_ctrl.value;
			break;
		case 'n':
			vio[0x178] = vio[0x178] | (1<<31);
			break;
		case 'i':
			init();
			break;
		case 'a':
			{
			Ref<net::Packet> p;
			net::request_packet(p);
			addreceive(p);
			}
			break;
		case '=':
			rctl.en = 1;
			tctl.en = 1;
			vio[0x100] = rctl.value;
			vio[0x400] = tctl.value;
			break;
		case 'W':
			vio[0x3818] = txq.tail;
			break;
		case 'w':
			{
			uint32_t txtail = txq.next();
			TRMT_DESC *txd = &descpage->trmt[txtail];
			txd->address = 0;
			txd->info = 0;
			descpage->trmt_slot[txtail].reset();
			}
			break;
		case '.':
			txq.pop();
			vio[0x3810] = txq.head; // T head
			break;
		case ',':
			rxq.pop();
			vio[0x2810] = rxq.head; // R head
			break;
		case '>':
			vio[0x3810] = txq.head; // T head
			break;
		case '<':
			vio[0x2810] = rxq.head; // R head
			break;
		case 'R':
			vio[0x2818] = rxq.tail;
			break;
		case 'r':
			{
			uint32_t rxtail = rxq.next();
			RECV_DESC *rxd = &descpage->recv[rxtail];
			rxd->address = 0;
			rxd->status = 0;
			}
			break;
		case 'g':
			r_ctrl.gio_dis ^= 1;
			vio[0] = r_ctrl.value;
			break;
		case 'l':
			r_ctrl.slu ^= 1;
			vio[0] = r_ctrl.value;
			break;
		case 'L':
			r_ctrl.lrst ^= 1;
			vio[0] = r_ctrl.value;
			break;
		}
	} else if(cmdl > 1) {
		for(size_t i = 0; i != rxq.limit;) {
			RECV_DESC *rxd = &descpage->recv[i];
			uint64_t rdbp = 0;
			if(descpage->recv_slot[i]) {
			rdbp = mem::translate_page((uintptr_t)descpage->recv_slot[i]->buf);
			}
			if((i & 3) == 0) {
				xiv::printf("e1000: RQ(%03d-%03d):", i, i + 3);
			}
			uint8_t sb = rxd->status;
			xiv::printf("  %010lx<>%010lx [%s%s%s%s%s%s%s%s]", rdbp, rxd->address,
			sb & 0x80 ? "P":"-",
			sb & 0x40 ? "I":"-",
			sb & 0x20 ? "T":"-",
			sb & 0x10 ? "4":"-",
			sb & 8 ? "V":"-",
			sb & 4 ? "X":"-",
			sb & 2 ? "E":"-",
			sb & 1 ? "D":"-"
			);
			i++;
			if((i & 3) == 0) {
				xiv::putc(10);
			}
		}
		for(size_t i = 0; i != txq.limit;) {
			TRMT_DESC *txd = &descpage->trmt[i];
			uint64_t tdbp = 0;
			if(descpage->trmt_slot[i]) {
			tdbp = mem::translate_page((uintptr_t)descpage->trmt_slot[i]->buf);
			}
			if((i & 3) == 0) {
				xiv::printf("e1000: TQ(%03d-%03d):", i, i + 3);
			}
			uint64_t txs = txd->info;
			uint8_t sb = (txs >> 32) & 0xf;
			uint8_t cb = (txs >> 24) & 0xff;
			xiv::printf("  %010lx<>%010lx [%s%s%s%s-%s%s%s%s%s%s%s%s]", tdbp, txd->address,
			sb & 8 ? "3":"-",
			sb & 4 ? "2":"-",
			sb & 2 ? "1":"-",
			sb & 1 ? "D":"-", // status
			cb & 0x80 ? "I":"-", // cmd
			cb & 0x40 ? "V":"-",
			cb & 0x20 ? "X":"-",
			cb & 0x01 ? "4":"-",
			cb & 8 ? "R":"-",
			cb & 4 ? "N":"-",
			cb & 2 ? "C":"-",
			cb & 1 ? "E":"-"
			);
			i++;
			if((i & 3) == 0) {
				xiv::putc(10);
			}
		}
	}
}

void e1000::processqueues() {
	HWR_IMS fint;
	fint.value = _ixa_xchg(&lastint, 0);
	if((fint.value & ~0x80000081)) { // ingore RXT0 or ASSERT
		xiv::printf("e1000: ICR: %x %s%s%s%s%s%s%s%s\n", fint,
			fint.txdw ? " TXDW" : "",
			fint.txqe ? " TXQE" : "",
			fint.lsc ? " LSC" : "",
			fint.rxseq ? " RXSEQ" : "",
			fint.rxdmt0 ? " RXDMT0" : "",
			fint.rxo ? " RXO" : "",
			fint.rxt0 ? " RXT0" : "",
			fint.txd_low ? " TXD-LOW" : ""
		);
		if(fint.lsc) {
			HWR_CTRL r_ctrl = { .value = vio[0] };
			HWR_STATUS dsr = { .value = vio[8] };
			r_ctrl.status();
			dsr.status();
			xiv::printf("e1000: CTRLEXT: %x\n", vio[0x18]);
			if(rxq.empty()) {
				Ref<net::Packet> p;
				for(size_t i = 0; i < 32; i++) { // initial load
					net::request_packet(p);
					addreceive(p);
				}
			}
		}
	}
	if(fint.txdw) {
		size_t qhead = txq.head;
		TRMT_DESC *txd = &descpage->trmt[qhead];
		Ref<net::Packet> *txrp = &descpage->trmt_slot[qhead];
		TRMT_UN txu;
		while(!txq.empty()) {
			txu.raw = txd->info;
			if(txu.exdata.dcmd & TDC_RS) {
				if(!(txu.exdata.status & TDS_DD)) break;
			}
			txrp->reset();
			qhead = txq.pop();
			txd = &descpage->trmt[qhead];
			txrp = &descpage->trmt_slot[qhead];
		}
	}
	if(fint.rxt0 || fint.rxo || fint.rxdmt0) {
		if(fint.rxo || fint.rxdmt0) {
			Ref<net::Packet> p;
			for(size_t i = 0; i < 8; i++) { // maybe stingy
				net::request_packet(p);
				addreceive(p);
			}
		}
		RECV_DESC *rxd = &descpage->recv[rxq.head];
		Ref<net::Packet> *rxrp = &descpage->recv_slot[rxq.head];
		while(!rxq.empty() && (rxd->status & 1)) {
			Ref<net::Packet> rxr = yuki::move(*rxrp);
			rxr->size = rxd->length;
			if(rxr) {
				//uint64_t ppa = mem::translate_page((uintptr_t)rxr->buf);
				//xiv::printf("e1000: recv: (%03d) %12lx <> %12lx\n", rxq.head, ppa, rxd->address);
				net::enqueue(rxr);
				if(rxr->ref_count != 1) { // recycle if not claimed
					request_packet(rxr);
				}
				// next
				addreceive(rxr);
			}
			size_t qhead = rxq.pop();
			rxd = &descpage->recv[qhead];
			rxrp = &descpage->recv_slot[qhead];
		}
	}
}
void e1000::remove() {
}
void e1000::transmit(Ref<net::Packet> pkt) {
	if(pkt->size < 42) {
		xiv::print("e1000: TX: Frame too small\n");
		return;
	}
	if(txq.full()) {
		xiv::print("e1000: TX Queue overflow\n");
		return;
	}
	if(!pkt->ref_count) {
		xiv::printf("e1000: trmt: packet has no refs\n");
	}
	uint32_t txtail = txq.next();
	TRMT_DESC *txd = &descpage->trmt[txtail];
	descpage->trmt_slot[txtail] = pkt;
	TRMT_UN txi;
	txi.raw = 0;
	txd->address = mem::translate_page((uintptr_t)pkt->buf);
	//xiv::printf("e1000: TX[%x] PG: %x\n", txtail, (uintptr_t)buf);
	txi.exdata.len = pkt->size;
	txi.exdata.dtype = 0x1;
	txi.exdata.status = 0;
	txi.exdata.dcmd = TDC_IDE | TDC_DEXT | TDC_RS | TDC_IFCS | TDC_EOP;
	txd->info = txi.raw;
	asm volatile("mfence");
	vio[0x3818] = txq.tail;
}

void e1000::addreceive(Ref<net::Packet> pkt) {
	if(rxq.full()) return;
	uint32_t rxtail = rxq.next();
	RECV_DESC *rxd = &descpage->recv[rxtail];
	pkt->size = 0;
	descpage->recv_slot[rxtail] = pkt;
	rxd->address = mem::translate_page((uintptr_t)pkt->buf);
	rxd->status = 0;
	asm volatile("mfence");
	//xiv::printf("e1000: add-rx: (%03d) %016lx\n", rxtail, rxd->address);
	vio[0x2818] = rxq.tail;
}

void e1000::getmediaaddr(uint8_t *p) const {
	for(int i = 0; i < 6; i++) p[i] = macaddr[i];
}

}
