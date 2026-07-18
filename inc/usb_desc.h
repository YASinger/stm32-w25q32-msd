#ifndef __USB_DESC_H
#define __USB_DESC_H

#include "stm32f10x.h"

#define MASS_SIZ_DEVICE_DESC              18
#define MASS_SIZ_CONFIG_DESC              32

extern const uint8_t MASS_DeviceDescriptor[MASS_SIZ_DEVICE_DESC];
extern const uint8_t MASS_ConfigDescriptor[MASS_SIZ_CONFIG_DESC];

#endif /* __USB_DESC_H */
