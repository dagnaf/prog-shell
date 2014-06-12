// main.c
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#define MAXCHAR 1024
#define MAXTOKEN 128
#define MAXPATH 64
char line[MAXCHAR];
char *tokens[MAXTOKEN];
int num_tokens;
char *builtin[] = { "cd", "clear", NULL };
char *paths[MAXPATH];
int fd[2][2];
char *cmd;
char *args[MAXTOKEN];
int cur;
int num_pipes;
pid_t pgid;
int bg;
int std_in;
int std_out;
void printMsg(const char *s) {
	printf("%s\n", s);
}
void prompt() {
	char wd[1024];
	printf("%s $ ", getcwd(wd, 1024));
}
void redirect(int i, char *in, char *out, int last) {
	pid_t pid;
	int my_fd;
	int j;
	if (out == NULL && num_pipes > 0 && i != num_pipes) {
		// puts("create fd");
		pipe(fd[i%2]);
	}
	pid = fork();
	if (pid == 0) {
		// dup2(std_in, STDIN_FILENO);
		// dup2(std_out, STDOUT_FILENO);
		if (in != NULL) {
			my_fd = open(in, O_RDONLY, 0600);
			dup2(my_fd, STDIN_FILENO);
			close(my_fd);
		} else if (num_pipes > 0 && i != 0) {
			// puts("pipe redirect stdin");
			dup2(fd[(i+1)%2][0], STDIN_FILENO);
		}
		if (out != NULL) {
			my_fd = open(out, O_CREAT | O_TRUNC | O_WRONLY, 0600);
			dup2(my_fd, STDOUT_FILENO);
			close(my_fd);
		} else if (num_pipes > 0 && i != num_pipes) {
			// puts("pipe redirect stdout");
			dup2(fd[i%2][1], STDOUT_FILENO);
		}
		// printf("%d %d\n", STDIN_FILENO, STDOUT_FILENO);
		// puts(cmd); exit(0);
		execvp(cmd, args);
	} else {
		// printf("[%d %d]\n", bg, last);
		// printf("create %d\n", pid);
		if (!bg) {
			if (pgid < 0) pgid = pid;
			setpgid(pid, pgid);
		}
		if (num_pipes != 0) {
			if (i != 0) close(fd[(i+1)%2][0]);
			if (i != num_pipes) close(fd[i%2][1]);
		}
		if (!bg && last) {
			while ((pid = waitpid(-pgid, NULL, 0)) > 0) {
				// printf("wait %d\n", pid);
			}
		}
	}
}
// void pip(int i)
// {
// 	pid_t pid;
// 	pid = fork();
// 	if (i != num_pipes) pipe(fd[i%2]);
// 	if (pid == 0) {
// 		if (i != num_pipes) dup2(fd[i%2][1], STDOUT_FILENO);
// 		if (i != 0) dup2(fd[(i+1)%2][0], STDIN_FILENO);
// 		execvp(cmd, args);
// 	} else {
// 		if (i != 0) close(fd[(i+1)%2][0]);
// 		if (i != num_pipes) close(fd[i%2][1]);
// 	}
// }
void command(int i) {
	int j = 0;
	char *in = NULL, *out = NULL;
	cmd = tokens[cur];
	while (tokens[cur] != NULL &&
		strcmp(tokens[cur], "<") != 0 &&
		strcmp(tokens[cur], ">") != 0 &&
		strcmp(tokens[cur], "|") != 0 &&
		strcmp(tokens[cur], "&") != 0) {
		args[j++] = tokens[cur++];
	}
	args[j] = NULL;
	if (tokens[cur] == NULL) {
		// puts("last cmd");
		redirect(i, NULL, NULL, 1);
		return;
	}
	if (strcmp(tokens[cur], "<") == 0) {
		in = tokens[cur+1];
		cur += 2;
		if (tokens[cur] != NULL && strcmp(tokens[cur], ">") == 0) {
			out = tokens[cur+1];
			cur += 2;
		}
		// redirect(i, in, out); command(i+1);
	} else if (strcmp(tokens[cur], ">") == 0) {
		out = tokens[cur+1];
		cur += 2;
		if (tokens[cur] != NULL && strcmp(tokens[cur], "<") == 0) {
			in = tokens[cur+1];
			cur += 2;
		}
	}
	// puts("aa");
	if (tokens[cur] == NULL) {
		redirect(i, in, out, 1);
	} else if (strcmp(tokens[cur], "&") == 0) {
		redirect(i, in, out, 1);
	} else if (strcmp(tokens[cur], "|") == 0) {
		// puts(in == NULL ? "NULL" : in);
		// puts(out == NULL ? "NULL" : out);
		redirect(i, in, out, 0);
		cur++;
		command(i+1);
	}
}
int fileExist(char *name) {
	int i;
	char file[100];
	if (name[0] == '/' || name[0] == '.') {
		if (access(name, F_OK) == 0) return 0;
		else return 1;
	}
	for (i = 0; builtin[i] != NULL; ++i) {
		if (strcmp(name, builtin[i]) == 0) return 0;
	}
	for (i = 0; paths[i] != NULL; ++i) {
		strcpy(file, paths[i]);
		strcat(file, "/");
		strcat(file, name);
		// printf("%s\n", file);
		if (access(file, F_OK) == 0) return 0;
	}
	// strcpy(file, "./");
	// strcat(file, name);
	// if (access(file, F_OK) == 0) return 0;
	return 1;
}
int judge() {
	int i;
	int in_a = -1;
	int out_a = -1;
	int pipe_a = -1;
	int pipe_b = -1;
	if (fileExist(tokens[0]) != 0) {
		printf("%s not exist\n", tokens[0]);
		return 1;
	}
	for (i = 1; tokens[i] != NULL; ++i) {
		if (strcmp(tokens[i], "&") == 0) {
			bg = 1;
			break;
		}
		if (strcmp(tokens[i-1], "<") == 0 ||
			strcmp(tokens[i-1], "|") == 0) {
			if (fileExist(tokens[i]) != 0) {
				printf("%s: not exist\n", tokens[i]);
				return 1;
			}
		}
		if (strcmp(tokens[i], "<") == 0 && in_a == -1) in_a = i;
		if (strcmp(tokens[i], ">") == 0 && out_a == -1) out_a = i;
		if (strcmp(tokens[i], "|") == 0 && pipe_a == -1) pipe_a = i;
		if (strcmp(tokens[i], "|") == 0) pipe_b = i;
		if (strcmp(tokens[i], "|") == 0) ++num_pipes;
	}
	if (strcmp(tokens[i-1], "<") == 0 ||
		strcmp(tokens[i-1], ">") == 0 ||
		strcmp(tokens[i-1], "|") == 0 ||
		(pipe_a != -1 && in_a != -1 && in_a > pipe_a) ||
		(pipe_a != -1 && out_a != -1 && out_a < pipe_b)) {
		printf("%d %d %d %d\n", in_a, out_a, pipe_a, pipe_b);
		printMsg("Syntax error");
		return 1;
	}
	return 0;
}
void initShell() {
	std_in = dup(STDIN_FILENO);
	std_out = dup(STDOUT_FILENO);
	int i = 1;
	char path[MAXCHAR];
	strcpy(path, (const char *)getenv("PATH"));
	if ((paths[0] = strtok(path, ":")) == NULL) return;
	while ((paths[i] = strtok(NULL, ":")) != NULL) ++i;
	// for (i = 0; paths[i] != NULL; ++i) printf("%s\n", paths[i]);
}
void cd(char* path) {
	// If we write no path (only 'cd'), then go to the home directory
	if (path == NULL) chdir(getenv("HOME")); 
	// Else we change the directory to the one specified by the 
	// argument, if possible
	else{ 
		if (chdir(path) == -1) {
			printf(" %s: no such directory\n", path);
   		}
	}
}
int main(int argc, char const *argv[])
{
	printMsg("welcome");
	initShell();
	while (1) {
		prompt();
		fgets(line, MAXCHAR, stdin);
		num_tokens = 1;
		cur = 0;
		pgid = -1;
		bg = 0;
		num_pipes = 0;
		if ((tokens[0] = strtok(line, " \t\n")) == NULL) {
			printf("\n");
			continue;
		}
		while ((tokens[num_tokens] = strtok(NULL, " \t\n")) != NULL) {
			++num_tokens;
		}
		// for (cur = 0; tokens[cur] != NULL; ++cur) puts(tokens[cur]); cur = 0;
		if (strcmp(tokens[0], "leave") == 0) break;
		else if (strcmp(tokens[0], "cd") == 0) cd(tokens[1]);
		else if (strcmp(tokens[0], "clear") == 0) system("clear");
		else {
			if (judge() == 0) command(0);
			else printMsg("Error");
		}
	}
	printMsg("goodbye");
	// kill(0, SIGKILL);
	return 0;
}