#include "Pokitto.h"
#include "SDFileSystem.h"   
#include "USBMSD_SD.h"
#include "ESPLoader.h"
#include <string>
 
using PC = Pokitto::Core;
using PD = Pokitto::Display;
using PB = Pokitto::Buttons;

enum State
{
    stateUSBDrive,
    stateConfirmFlashing,
    stateFlashESP,
    stateFlashingESPFinished,
};

USBMSD_SD* usbmsd_sd = nullptr;
SDFileSystem *sdFs = nullptr;
uint32_t prevBlock_read = 0;
uint32_t prevBlock_write = 0;
const int32_t margin = 14;
const std::string ESPFlashfileName = "PokiPlusWifiLib.espfirm";
uint32_t* MAGIC_ADDRESS = (uint32_t*)0xE000ED0C;
const uint32_t RESTART_MCU = 0x05FA0004;
int32_t count=0;
int32_t state=stateUSBDrive;
bool firstTime = true;

void DrawPanel(int16_t x, int16_t y, int16_t w, int16_t h);
template<class T>
void PrintToStatusArea(int8_t color, T value);
bool SDInit();
bool flashFirmware(std::string path, const uint32_t flash_offset);
void PrintProgressBar(int32_t x, int32_t y, int32_t w, int32_t h, int8_t color, int8_t percentage);

void init() 
{
    Pokitto::Sound::ampEnable(0);
    PD::persistence = true;
    PD::adjustCharStep = 0;
    PD::loadRGBPalette((unsigned char*)palettePico);
    PD::invisiblecolor = 0;
    PD::setFont(fontZXSpec);

    // Draw the screen
    PD::setColor(13,0);
    PD::fillRect(0, 0, 220, 176);
    PD::setColor(9);  // orange
    PD::print(margin,3,"*** ESP FLASHER ***\n\n");
    PD::update();
    
    // Wait until the user releases the A button.
    PB::update();
    while(PB::aBtn())
        PB::update();
        
    // Check the ESP flash image existence.
    // Init the SD card.
    bool ok = SDInit();
    if(ok)
    {
        // Check the file existence.
        wait_ms(2000);
        FileHandle *file=sdFs->open(ESPFlashfileName.c_str(), O_RDWR );
        if(file)
        {
            // Found ESP flash file. Start flashing.
            state=stateConfirmFlashing;
            file->close();
        }
    }
    // Delete SDFS
    if(sdFs)
    {
        sdFs->unmount();
        delete(sdFs);
        sdFs = nullptr;
    }
}


void update() 
{
    if(PB::pressed(BTN_A))
    {
        //if(state==stateUSBDrive) state=stateConfirmUSBCableDisconnected;
        if(state==stateConfirmFlashing) state=stateFlashESP;
        
        // Restart Pokitto to be able to flash the ESP binary. If the restart is not made
        // after the USB drive we cannot connect to ESP for some reason!
        if(state==stateUSBDrive) *MAGIC_ADDRESS = RESTART_MCU;
    }
    else if(PB::pressed(BTN_B)) 
    {
        if(state==stateConfirmFlashing) state=stateUSBDrive;
    }
    else if(PB::pressed(BTN_C) )
    {
        // Jump to the loader.
        
        // Disconnect USB disk
        if(usbmsd_sd)
        {
            usbmsd_sd->disconnect();
            delete(usbmsd_sd);
            usbmsd_sd = nullptr;
            wait_ms(500);
        }

        // Delete SDFS
        if(sdFs)
        {
            sdFs->unmount();
            delete(sdFs);
            sdFs = nullptr;
        }
        
        // Start the app loader
        PC::jumpToLoader();
    }
        
    // *** Handle states
        
    if(state==stateUSBDrive)  // USB drive state 
    {
        PD::setColor(13,0);
        PD::fillRect(0, 0, 220, 176);
        PD::setCursor(0,0);
    
        int32_t startY = 20;
        DrawPanel(5, startY, 220-10, 176-60);
        PD::setColor(9);  // orange
        PD::print(margin,3,"*** ESP FLASHER ***\n\n");
        PD::setColor(7);  // white
        PD::println(margin, startY+3,    "Connect the USB cable and wait");
        PD::println(margin, PD::cursorY, "for the USB drive to be ready");
        PD::println(margin, PD::cursorY, "on PC. Then copy the ESP file");
        PD::println(margin, PD::cursorY, "from PC to Pokitto.");
        PD::println("");
        PD::println(margin, PD::cursorY, "The ESP flash file name should be");
        PD::setColor(10);  // yellow
        PD::println(margin, PD::cursorY, ESPFlashfileName.c_str());
        PD::println("");
        PD::setColor(7);  // white
        PD::println(margin, PD::cursorY, "When the file has been");
        PD::println(margin, PD::cursorY, "copied, press A");
    
        PD::setColor(10);  // yellow
        if(firstTime)
            PD::println(margin, 120, "A: File copied");
        else
            PD::println(margin, 120, "A: File copied      C: Cancel");
        
        // Print to status area
        int32_t statusAreaY = 140;
        if(firstTime)
        {
            PD::setColor(8);  // red
            PD::print(margin,statusAreaY, "Connect the USB cable !");
        }
        else if(block_write!=prevBlock_write)
        {
            PD::setColor(11);  // l.green
            PD::print(margin,statusAreaY, "Writing to SD: ");
            uint32_t now = PC::getTime();
            if((now / 500)%3 == 0)
                PD::println(".    .");
            else if((now / 500)%3 == 1)
                PD::println(" .  . ");
            else 
                PD::println("  ..  ");
            //PD::println(block_write);
            prevBlock_write = block_write;
        }
        else if(block_read!=prevBlock_read)
        {
            PD::setColor(11);  // l.green
            PD::print(margin,statusAreaY, "Reading from SD: ");
            uint32_t now = PC::getTime();
            if((now / 500)%3 == 0)
                PD::println(".    .");
            else if((now / 500)%3 == 1)
                PD::println(" .  . ");
            else 
                PD::println("  ..  ");
            //PD::println(block_read);
            prevBlock_read = block_read;
        }

        PD::update();
        
        if(firstTime)
        {
            // Start USB disk.
            // Note, this call blocks until the cable is connected!
            //PD::print("USBMSD_SD called\n");
            usbmsd_sd = new USBMSD_SD(P0_9, P0_8, P0_6, P0_7); // P0_9, P0_8, P0_6, P0_7 = pins for SD card
            //PD::print("USBMSD_SD done\n");
            firstTime = false;
        }
    }  // end if state==stateUSBDrive

    else if(state==stateConfirmFlashing)  // Disconnect cable view 
    {
        PD::setColor(13,0);
        PD::fillRect(0, 0, 220, 176);
        PD::setCursor(0,0);
        PD::setColor(9);  // orange
        PD::print(margin,3,"*** ESP FLASHER ***\n\n");
    
        int32_t startY = 20;
        DrawPanel(5, startY+40, 220-10, 176-60-40);
        PD::setColor(7);
        PD::println(margin, startY+50+10, "The ESP flash file found in SD:");
        PD::println(margin+10, PD::cursorY, ESPFlashfileName.c_str());
        PD::println();
        PD::println(margin, PD::cursorY,  "Press A to prodeed to flashing.");
     
        PD::setColor(10);  // yellow
        PD::println(margin, 120, "A: Flash ESP   B: Start USB drive");
        
        PD::update();
        
    } // end if state==stateConfirmUSBCableDisconnected
    
    else if(state==stateFlashESP)  // ESP flashing state 
    {
        PD::setColor(13,0);
        PD::fillRect(0, 0, 220, 176);
        PD::setCursor(0,0);
    
        int32_t startY = 20;
        DrawPanel(5, startY, 220-10, 176-60);
        PD::setColor(9);  // orange
        PD::print(margin,3,"*** ESP FLASHER ***\n\n");
        PD::setColor(7);
        PD::println(margin, startY+10, "Flashing ESP: ");

        PD::update();
        
        // Print to status area
        PrintToStatusArea(11, "Disconnecting USB");
        PD::update();
        
         // Disconnect USB disk
        if(usbmsd_sd)
        {
            usbmsd_sd->disconnect();
            delete(usbmsd_sd);
            usbmsd_sd = nullptr;
        }
        
        wait_ms(3000);
        
        // Print to status area
        PrintToStatusArea(11, "Init SD card");
        PD::update();
        
        // Init SD card
        bool ok = SDInit();
        if(ok)
        {
            wait_ms(2000);
            ok = flashFirmware(ESPFlashfileName, 0);
        }
        
        // Print to status area
        if(ok)
            PrintToStatusArea(11, "ESP flashing done!");
        PD::update();

        state=stateFlashingESPFinished; // finished
        if(!ok) for(;;); // Loop forever in case of error.
        
    } // end if state==stateFlashESP
    
    else if(state==stateFlashingESPFinished)  // Flashing done. 
    {
        PD::setColor(13,0);
        PD::fillRect(0, 0, 220, 176);
        PD::setCursor(0,0);
    
        int32_t startY = 20;
        DrawPanel(5, startY, 220-10, 176-60);
        PD::setColor(9);  // orange
        PD::print(margin,3,"*** ESP FLASHER ***\n\n");
        PD::setColor(7);
        PD::println(margin, startY+10, "ESP flashing succeeded!");
    
        PD::setColor(10);  // yellow
        PD::println(margin, 120, "C: Start loader");
        
        PD::update();
        
    } // end if state==stateFlashingESPFinished
}

void DrawPanel(int16_t x, int16_t y, int16_t w, int16_t h)
{
    // Draw panel shadow
    PD::color = 1; // dark blue
    PD::fillRect(x, y, w-2, h);

    // Draw panel highlight
    PD::color = 6;
    PD::fillRect(x, y, w-4, h-2);

    // Draw panel
    PD::color = 12; // blue
    PD::fillRect(x+2, y+2, w-6, h-4);
}

template<class T>
void PrintToStatusArea(int8_t color, T value)
{
    // Print to status area
    int32_t statusAreaY = 140;
    PD::setColor(13,0);
    PD::fillRect(0, statusAreaY, 220, 176-statusAreaY);
    
    PD::setCursor(margin, statusAreaY);
    PD::setColor(color); 
    PD::print(value);
}        

void PrintProgressBar(int32_t x, int32_t y, int32_t w, int32_t h, int8_t color, int8_t percentage)
{
    // Clear area
    PD::color = 12; // blue
    PD::fillRect(x, y, w, h);
    
    // Draw the bar
    PD::setColor(color);
    PD::drawRect(x, y, w, h);
    int32_t w2 = (percentage*(w-4))/100;
    PD::fillRect(x+2, y+2, w2, h-4);
}        

bool SDInit()
{
    if(sdFs) return true;
    
    sdFs = new SDFileSystem( P0_9, P0_8, P0_6, P0_7, "sd", NC, SDFileSystem::SWITCH_NONE, 25000000 );
    sdFs->crc(false);
    sdFs->write_validation(false);
    //sdFs->large_frames(true);

    if(sdFs->card_type()==SDFileSystem::CardType::CARD_UNKNOWN)
    {
        // Print to status area
        PrintToStatusArea(8, "SD card initialization failed!");
        PD::update();
        return false;
    }
    
    return true;
}

bool flashFirmware(std::string path, const uint32_t flash_offset)
{
    FileHandle *file=sdFs->open(path.c_str(), O_RDWR );
    
    if(!file)
    {
        PrintToStatusArea(8, "File open failed");
        PD::update();
        return false;
    }
    ESPLoader Loader(230400);//460800

    uint32_t fsize=file->flen();
    PrintToStatusArea(11, "Connecting to ESP8266 Module");
    PD::update();
    Loader.enterBootLoader();
    wait_ms(1000);

    if(Loader.sync())
    {
        if(Loader.flash_begin(fsize, flash_offset))
        {
            uint8_t data[Loader.FLASH_WRITE_SIZE];
            uint32_t parts=(fsize+Loader.FLASH_WRITE_SIZE-1)/Loader.FLASH_WRITE_SIZE;
            for(auto i=0;i<parts;i++)
            {
                // Draw status area text.
                PrintToStatusArea(11, "Flashing Firmware: ");
                PD::setColor(7);
                PD::print((100*i)/parts);
                PD::print(" %");
                
                // Draw the progress bar.
                PrintProgressBar(margin, 73, 220-(margin*2), 20, 7, (100*i)/parts);

                PD::update();
                uint32_t count=file->read(data, Loader.FLASH_WRITE_SIZE);
                if(!Loader.flash_block(data, i, count ))
                {
                    PrintToStatusArea(8, "Sending data to ESP8266 Module Failed");
                    PD::update();
                    return false;
                }
            }
            file->close();
            Loader.flash_end(true);
            
            // Remove the flash file.
            sdFs->remove(path.c_str());

            PrintToStatusArea(11, "Firmware flashed Successfully");
            PD::update();
            return true;
            
        }
        else
            PrintToStatusArea(8, "Flash Erase Failed");

    }
    else
        PrintToStatusArea(8, "Can't connect ESP8266 Module");

    PD::update();
    
    return false;
}



