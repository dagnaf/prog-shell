void printMsg(const char *s);
void prompt();
void redirect(int i, char *in, char *out, int last);
void command(int i);
int fileExist(char *name);
int judge();
void initShell();
void cd(char* path);
void run();