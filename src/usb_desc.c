#include "usb_desc.h"

/* ── 设备描述符 (18 字节) ─────────────────────────────────────────────────── */
const uint8_t MASS_DeviceDescriptor[MASS_SIZ_DEVICE_DESC] = {
    0x12,   /* bLength           = 18                            */
    0x01,   /* bDescriptorType   = DEVICE                        */
    0x00,   /* bcdUSB, version 2.00                              */
    0x02,
    0x00,   /* bDeviceClass    = 0 (接口级别定义)                 */
    0x00,   /* bDeviceSubClass                                    */
    0x00,   /* bDeviceProtocol                                    */
    0x40,   /* bMaxPacketSize0 = 64                              */
    0x83,   /* idVendor        = 0x0483 (STMicroelectronics)     */
    0x04,
    0x20,   /* idProduct       = 0x5720                          */
    0x57,
    0x00,   /* bcdDevice       = 2.00                            */
    0x02,
    1,      /* iManufacturer                                     */
    2,      /* iProduct                                          */
    3,      /* iSerialNumber                                     */
    0x01    /* bNumConfigurations                                */
};
