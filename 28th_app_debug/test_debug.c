/*
 * =====================================================================================
 *
 *       Filename:  test_debug.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  04/11/2015 09:06:30 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdio.h>


int C(int *p)
{
	*p = 0x12;
}

int B(int *p)
{
	C(p);
}

int A(int *p)
{
	B(p);
}

int A2(int *p)
{
	C(p);
}

int main(int argc, char *argv)
{
	int a;
	int *p = NULL;

	A2(&a);						//A2 -> C
	printf("a = 0x%x\n", a);
	
	A(p);						//A -> B -> C
	printf("*p = 0x%x\n", *p);
	
	return 0;
}
