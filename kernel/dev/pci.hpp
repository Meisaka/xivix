#ifndef PCI_HAI
#define PCI_HAI

#include "ktypes.hpp"

namespace hw {

class PCI final {
public:
	PCI();
	~PCI();

	static uint16_t config_readw(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
};

} // ns hw

#endif

