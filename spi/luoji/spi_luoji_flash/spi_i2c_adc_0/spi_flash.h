#ifndef _SPI_FLASH
#define _SPI_FLASH

void spi_flash_init(void);
void spi_flash_read_id(unsigned int *pMID, unsigned int *pDID);

#endif

