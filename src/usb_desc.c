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

/* ── 配置描述符 (32 字节) ─────────────────────────────────────────────────── */
const uint8_t MASS_ConfigDescriptor[MASS_SIZ_CONFIG_DESC] = {
    /******** 配置描述符 (9B) ********/
    0x09,   /* bLength                                          */
    0x02,   /* bDescriptorType = CONFIGURATION                  */
    MASS_SIZ_CONFIG_DESC,                                       /* wTotalLength    */
    0x00,
    0x01,   /* bNumInterfaces = 1                               */
    0x01,   /* bConfigurationValue = 1                          */
    0x00,   /* iConfiguration                                   */
    0xC0,   /* bmAttributes  = 自供电                           */
    0x32,   /* bMaxPower     = 100 mA                           */

    /******** 接口描述符 (9B) ********/
    0x09,   /* bLength                                          */
    0x04,   /* bDescriptorType = INTERFACE                      */
    0x00,   /* bInterfaceNumber                                 */
    0x00,   /* bAlternateSetting                                */
    0x02,   /* bNumEndpoints                                    */
    0x08,   /* bInterfaceClass    = Mass Storage                */
    0x06,   /* bInterfaceSubClass = SCSI transparent            */
    0x50,   /* bInterfaceProtocol = Bulk-Only Transport         */
    4,      /* iInterface                                       */

    /******** 端点 1 IN 描述符 (7B) ********/
    0x07,   /* bLength                                          */
    0x05,   /* bDescriptorType = ENDPOINT                       */
    0x81,   /* bEndpointAddress = EP1, IN                       */
    0x02,   /* bmAttributes      = Bulk                         */
    0x40,   /* wMaxPacketSize    = 64                           */
    0x00,
    0x00,   /* bInterval                                       */

    /******** 端点 2 OUT 描述符 (7B) ********/
    0x07,   /* bLength                                          */
    0x05,   /* bDescriptorType = ENDPOINT                       */
    0x02,   /* bEndpointAddress = EP2, OUT                      */
    0x02,   /* bmAttributes      = Bulk                         */
    0x40,   /* wMaxPacketSize    = 64                           */
    0x00,
    0x00    /* bInterval                                       */
};
