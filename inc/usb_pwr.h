#ifndef __USB_PWR_H
#define __USB_PWR_H

#include "stm32f10x.h"
#include "usb_type.h"

typedef enum _RESUME_STATE {
    RESUME_OFF = -1,
    RESUME_ESOF = 0,
    RESUME_EXTERNAL,
    RESUME_INTERNAL,
    RESUME_LATER,
    RESUME_WAIT,
    RESUME_ON
} RESUME_STATE;

typedef enum _DEVICE_STATE {
    UNCONNECTED = 0,
    ATTACHED    = 1,
    POWERED     = 2,
    SUSPENDED   = 3,
    ADDRESSED   = 4,
    CONFIGURED  = 5
} DEVICE_STATE;

extern __IO uint32_t bDeviceState;
extern __IO bool     fSuspendEnabled;

void PowerOn(void);
void PowerOff(void);
void Suspend(void);
void Resume(RESUME_STATE eResumeSetVal);

#endif /* __USB_PWR_H */
