#include "head.h"
int main(int argc, char const *argv[])
{
	printMsg("welcome");
	initShell();
	run();
	printMsg("goodbye");
	return 0;
}