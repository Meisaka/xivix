
#include "pci.hpp"

namespace hw {

PCI::PCI() {
}
PCI::~PCI() {
}

uint16_t PCI::config_readw(uint8_t, uint8_t, uint8_t, uint8_t) {
	return 0xffff;
}

} // ns hw

