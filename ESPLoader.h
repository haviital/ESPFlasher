#pragma once
#include <mbed.h>

struct sSlipHeader
{
    uint8_t Direction;
    uint8_t Command;
    uint16_t Size;
    uint32_t Value;
};

class SLIP
{
public:

    static constexpr uint8_t FRAME_DELIMITER=0xC0;
    
    static void setUART(Serial* uart);
    static void sendFrameDelimiter(void);
    static void sendFrameByte(uint8_t byte);
    static void sendFrameBuf(const void *data, const size_t size);
    static void sendPacket(const sSlipHeader &head, const void *data);
    static uint8_t recvFrameByte(void);
    static bool recvPacket(sSlipHeader &header, uint8_t *data, const size_t size);
    
private:
    static Serial* m_puart;
};

Serial* SLIP::m_puart=nullptr;

void SLIP::setUART(Serial* uart)
{
    m_puart=uart;
};

void SLIP::sendFrameDelimiter(void)
{
    m_puart->putc(FRAME_DELIMITER);
};

void SLIP::sendFrameByte(uint8_t byte)
{
    if(byte==FRAME_DELIMITER)
    {
        m_puart->putc(0xDB);
        m_puart->putc(0xDC);
    }
    else if(byte==0xDB)
    {
        m_puart->putc(0xDB);
        m_puart->putc(0xDD);
    }
    else
        m_puart->putc(byte);
    
};

void SLIP::sendFrameBuf(const void *data, size_t size)
{
    const uint8_t *buf_c = reinterpret_cast<const uint8_t *>(data);
    for(int i = 0; i < size; i++)
        sendFrameByte(buf_c[i]);
}

void SLIP::sendPacket(const sSlipHeader &head, const void *data)
{
    sendFrameDelimiter();
    sendFrameBuf(&head, sizeof(sSlipHeader));
    sendFrameBuf(data, head.Size);
    sendFrameDelimiter();
}

uint8_t SLIP::recvFrameByte(void)
{
    uint8_t byte=m_puart->getc();
    if(byte==0xDB)
    {
        uint8_t byte2=m_puart->getc();
        if(byte2==0xDC)
            return 0xC0;
        if(byte2==0xDD)
            return 0xDB;
        return -1;
    }
    else
        return byte;
};

bool SLIP::recvPacket(sSlipHeader &header, uint8_t *data, const size_t size)
{
    uint32_t timeout=20000;
    size_t start=Pokitto::Core::getTime();
    while((Pokitto::Core::getTime()-start) < timeout)
    {
        if(m_puart->readable()>0)
        {
            uint8_t byte=m_puart->getc();
            if(byte==FRAME_DELIMITER)
            {
                Pokitto::Display::update();
                uint8_t* pHeader=reinterpret_cast<uint8_t*>(&header);
                for(int i=0;i<sizeof(sSlipHeader);i++)
                    pHeader[i]=recvFrameByte();
                if(size < header.Size)
                    return false;
                for(int i=0;i<header.Size;i++)
                    data[i]=recvFrameByte();
                if(byte==FRAME_DELIMITER)
                    return true;
                else
                    return false;
            }
            else
                return false;
        }
    }
    return false;
};


class ESPLoader
{
public:

    enum eCommands: uint8_t
    {
        FLASH_BEGIN = 0x02,
        FLASH_DATA  = 0x03,
        FLASH_END   = 0x04,
        MEM_BEGIN   = 0x05,
        MEM_END     = 0x06,
        MEM_DATA    = 0x07,
        SYNC        = 0x08,
        WRITE_REG   = 0x09,
        READ_REG    = 0x0a,
    };
    static constexpr uint8_t ROM_INVALID_RECV_MSG=0xD4;
    static constexpr uint32_t FLASH_WRITE_SIZE=0x400;//1 KB
    static constexpr uint8_t ESP_CHECKSUM_MAGIC=0xEF;
    
    static constexpr uint32_t FLASH_SECTOR_SIZE=0x1000;

    ESPLoader(uint32_t _baud);
    
    void enterBootLoader(void);
    
    bool sync(void);
    bool flash_begin(const uint32_t size, const uint32_t flash_offset=0x00000);
    bool flash_block(const void* data, const uint32_t num_seq, const uint32_t size=FLASH_WRITE_SIZE);
    bool flash_end(const bool reboot=true);

private:
    Serial m_uart;
    DigitalOut esp_pinEnable;
    DigitalOut esp_pinReset;
    DigitalOut esp_pinProg;
    
    void m_flushRX(void);
    uint32_t m_getEraseSize(const uint32_t offset, const uint32_t size);
    uint32_t m_checksum(const uint8_t *data, const uint32_t size);
};


ESPLoader::ESPLoader(uint32_t _baud): m_uart(USBTX, USBRX),esp_pinEnable(P0_21), esp_pinReset(P0_20), esp_pinProg(P1_1)
{
    m_uart.baud(_baud);//74800
    SLIP::setUART(&m_uart);
}

void ESPLoader::enterBootLoader(void)
{
    esp_pinEnable = 0;
	esp_pinProg = 0;
	esp_pinReset = 1;
	wait_ms(100);
	esp_pinEnable = 1;
}

bool ESPLoader::sync(void)
{
    m_flushRX();
    sSlipHeader syncHeader;
    std::memset(&syncHeader, 0, sizeof(sSlipHeader));
    syncHeader.Command=static_cast<uint8_t>(eCommands::SYNC);
    syncHeader.Size=36;
    uint8_t data[36];
    std::memset(data, 0x55, 36);
    data[0]=0x07;
    data[1]=0x07;
    data[2]=0x12;
    data[3]=0x20;
    
    sSlipHeader responseHeader;
    uint8_t responseData[4];
    
    for(int i=0;i<3;i++)
        SLIP::sendPacket(syncHeader, data);
    

    if(SLIP::recvPacket(responseHeader, responseData, 4))
    {
        if(responseHeader.Command==static_cast<uint8_t>(eCommands::SYNC) && responseHeader.Direction==1)
        {
            if(responseData[0]==0)
                return true;
        }
    }

    return false;
}

bool ESPLoader::flash_begin(const uint32_t size, const uint32_t flash_offset)
{
    uint32_t erase_size = m_getEraseSize(flash_offset, size);
    uint32_t num_data_packets = (size+FLASH_WRITE_SIZE-1)/FLASH_WRITE_SIZE;
    uint32_t packet_size=FLASH_WRITE_SIZE; 

    sSlipHeader fbHeader;
    std::memset(&fbHeader, 0, sizeof(sSlipHeader));
    fbHeader.Command=static_cast<uint8_t>(eCommands::FLASH_BEGIN);
    fbHeader.Size=16;
    uint8_t data[16];
    std::memcpy(data, &erase_size, sizeof(uint32_t));
    std::memcpy(data+sizeof(uint32_t), &num_data_packets, sizeof(uint32_t));
    std::memcpy(data+sizeof(uint32_t)*2, &packet_size, sizeof(uint32_t));
    std::memcpy(data+sizeof(uint32_t)*3, &flash_offset, sizeof(uint32_t));
    
    sSlipHeader responseHeader;
    uint8_t responseData[4];
    m_flushRX();
    SLIP::sendPacket(fbHeader, data);
    
    if(SLIP::recvPacket(responseHeader, responseData, 4))
    {
        if(responseHeader.Command==static_cast<uint8_t>(eCommands::FLASH_BEGIN) && responseHeader.Direction==1)
        {
            if(responseData[0]==0)
                return true;
        }
    }

    return false;
}

bool ESPLoader::flash_block(const void* data, const uint32_t num_seq, const uint32_t size)
{
    m_flushRX();
    sSlipHeader fdHeader;
    std::memset(&fdHeader, 0, sizeof(sSlipHeader));
    fdHeader.Command=static_cast<uint8_t>(eCommands::FLASH_DATA);
    fdHeader.Size=size+sizeof(uint32_t)*4;
    fdHeader.Value=m_checksum(reinterpret_cast<const uint8_t*>(data), size);
    
    uint8_t hData[sizeof(uint32_t)*4];
    
    std::memcpy(hData, &size, sizeof(uint32_t));
    std::memcpy(hData+sizeof(uint32_t), &num_seq, sizeof(uint32_t));


    SLIP::sendFrameDelimiter();
    SLIP::sendFrameBuf(&fdHeader, sizeof(sSlipHeader));
    SLIP::sendFrameBuf(hData, sizeof(uint32_t)*4);
    SLIP::sendFrameBuf(data, size);
    SLIP::sendFrameDelimiter();
    
    
    sSlipHeader responseHeader;
    uint8_t responseData[4];

    if(SLIP::recvPacket(responseHeader, responseData, 4))
    {
        if(responseHeader.Command==static_cast<uint8_t>(eCommands::FLASH_DATA) && responseHeader.Direction==1)
        {
            if(responseData[0]==0)
                return true;
        }
    }

    return false;
}

bool ESPLoader::flash_end(const bool reboot)
{
    uint32_t reboot32=reboot?0:1;
    
    m_flushRX();
    sSlipHeader feHeader;
    std::memset(&feHeader, 0, sizeof(sSlipHeader));
    feHeader.Command=static_cast<uint8_t>(eCommands::FLASH_END);
    feHeader.Size=4;
    uint8_t data[4];
    
    std::memcpy(data, &reboot32, sizeof(uint32_t));

    
    sSlipHeader responseHeader;
    uint8_t responseData[4];
    
    SLIP::sendPacket(feHeader, data);
    

    if(SLIP::recvPacket(responseHeader, responseData, 4))
    {
        if(responseHeader.Command==static_cast<uint8_t>(eCommands::FLASH_END) && responseHeader.Direction==1)
        {
            if(responseData[0]==0)
                return true;
        }
    }

    return false;
}


void ESPLoader::m_flushRX(void)
{
    while(m_uart.readable())
        m_uart.getc();
}

uint32_t ESPLoader::m_getEraseSize(const uint32_t offset, const uint32_t size)
{
    auto sectors_per_block=16;
    auto sector_size = FLASH_SECTOR_SIZE;
    auto num_sectors = (size+ sector_size-1)/sector_size;
    auto start_sector = offset/sector_size;
    
    auto head_sectors = sectors_per_block - (start_sector%sectors_per_block);
    if(num_sectors <(2*head_sectors))
        return (num_sectors+1)/(2*sector_size);
    else
        return (num_sectors-head_sectors)*sector_size;
    
}

uint32_t ESPLoader::m_checksum(const uint8_t *data, const uint32_t size)
{
    uint32_t checksum=ESP_CHECKSUM_MAGIC;
    for(int i=0;i<size;i++)
        checksum ^= data[i];
    return checksum;
}
