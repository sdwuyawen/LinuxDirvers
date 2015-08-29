#include "stdio.h"

int main(void)
{
	int var[2] = {0 ,0};
	int *pint;

	pint = (int *)((char *)var + 1);


	*var = 0x11223344;
	*pint = 0x55667788;

	printf("&var = %p\n", var);
	printf("pint = %p\n", pint);

	printf("var[0] = %08x\n", var[0]);
	printf("var[1] = %08x\n", var[1]);
	printf("var[1] = %08x\n", var[1]);

	return 0;

}
