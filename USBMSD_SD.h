/* mbed USBMSD_SD Library, for providing file access to SD cards
 * Copyright (c) 2008-2010, sford
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef USBMSD_SD_H
#define USBMSD_SD_H

#include "mbed.h"
#include "USBMSD.h"

extern uint32_t block_write;
extern uint8_t count_write;
extern const uint8_t* dataPtr_write;
extern int ret_write;

extern uint32_t block_read;
extern uint8_t count_read;
extern const uint8_t* dataPtr_read;
extern int ret_read;

extern int32_t reqType;
extern int32_t request;
extern uint32_t request_remaining;
extern bool EPBULK_OUT_callback_ok;
extern bool EPBULK_IN_callback_ok;
extern bool USBCallback_request_ok;
extern bool USBCallback_setConfiguration_ok;


/** Use the SDcard as mass storage device using the USBMSD class
 *
 * @code
 * #include "mbed.h"
 * #include "USBMSD_SD.h"
 *
 * USBMSD_SD sd(p5, p6, p7, p8);
 *
 * int main() {
 *   while(1);
 * }
 *
 * @endcode
 */
class USBMSD_SD : public USBMSD {
public:

    /** Create the File System for accessing an SD Card using SPI
     *
     * @param mosi SPI mosi pin connected to SD Card
     * @param miso SPI miso pin conencted to SD Card
     * @param sclk SPI sclk pin connected to SD Card
     * @param cs   DigitalOut pin used as SD Card chip select
     * @param name The name used to access the virtual filesystem
     */
    USBMSD_SD(PinName mosi, PinName miso, PinName sclk, PinName cs);
    virtual int disk_initialize();
    virtual int disk_status();
    //virtual int disk_read(uint8_t * buffer, uint64_t block_number);
    virtual int disk_read(uint8_t* data, uint64_t block, uint8_t count) override;  // without "=0" !
    virtual int disk_write(const uint8_t* data, uint64_t block, uint8_t count) override;
    //virtual int disk_write(const uint8_t * buffer, uint64_t block_number);
    virtual int disk_sync();
    virtual uint64_t disk_sectors();
    
    virtual uint64_t disk_size(){return _sectors*512;};
    
    
public:

    /*
    * Callback called when a packet is received
    */
    virtual bool EPBULK_OUT_callback();

    /*
    * Callback called when a packet has been sent
    */
    virtual bool EPBULK_IN_callback();

    /*
    * Set configuration of device. Add endpoints
    */
    virtual bool USBCallback_setConfiguration(uint8_t configuration);

    /*
    * Callback called to process class specific requests
    */
    virtual bool USBCallback_request();

    

protected:

    int _cmd(int cmd, int arg);
    int _cmdx(int cmd, int arg);
    int _cmd8();
    int _cmd58();
    int initialise_card();
    int initialise_card_v1();
    int initialise_card_v2();
    
    int _read(uint8_t * buffer, uint32_t length);
    int _write(const uint8_t *buffer, uint32_t length);
    uint64_t _sd_sectors();
    uint64_t _sectors;
    
    uint8_t _status;
    
    SPI _spi;
    DigitalOut _cs;
    int cdv;
};

#endif
