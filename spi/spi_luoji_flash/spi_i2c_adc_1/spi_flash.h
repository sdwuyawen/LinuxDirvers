#ifndef _SPI_FLASH
#define _SPI_FLASH

void spi_flash_init(void);
void spi_flash_read_id(unsigned int *pMID, unsigned int *pDID);
void spi_flash_erase_sector(unsigned int addr);
void spi_flash_page_program(unsigned int addr, unsigned char *buf, unsigned int len);
void spi_flash_read(unsigned int addr, unsigned char *buf, unsigned int len);

#endif

