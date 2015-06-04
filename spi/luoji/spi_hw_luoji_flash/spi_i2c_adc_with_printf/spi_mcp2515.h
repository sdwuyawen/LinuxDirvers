#ifndef	_SPI_MCP2515_H_
#define 	_SPI_MCP2515_H_

void spi_mcp2515_init(void);
unsigned char spi_mcp2515_read_reg(unsigned addr);

#endif