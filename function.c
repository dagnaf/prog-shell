#define MAXCHAR 1024
#define MAXTOKEN 128
#define MAXPATH 64
#include <stdio.h> // file,input,output
#include <unistd.h> // const
#include <stdlib.h> // exit
#include <string.h> // strcmp/cat/cpy
#include <sys/types.h> // pid
#include <sys/wait.h> // wait
#include <fcntl.h> // access

char line[MAXCHAR]; // cmd line
char *tokens[MAXTOKEN]; // cmd tokens
char *args[MAXTOKEN]; // cmd args (including cmd itself)
char *paths[MAXPATH]; // env path
int num_tokens; // num of tokens
char *builtin[] = { "cd", "clear", NULL }; // built-in cmd
int fd[2][2]; // alternative multi pipes
char *cmd; // pointer for cmd
int cur; // cur token
int num_pipes; // num of pipes
pid_t pgid; // process group id
int bg; // background exe signal

// print default msg without args
void printMsg(const char *s) {
	printf("%s\n", s);
}
// prompt line for current work directory
void prompt() {
	char wd[1024];
	printf("%s $ ", getcwd(wd, 1024));
}
/********************************************************
  args:
  i - current cmd, not args or | or > etc.
  in - cmd's stdin. if not redirected NULL
  out - cmnd's stdout. if not redirected NULL
  last - is last cmd? 
********************************************************/
void redirect(int i, char *in, char *out, int last) {
	pid_t pid; // new child process
	int my_fd; // redirect to file
	// if redirect to pipe and not last cmd
	if (out == NULL && num_pipes > 0 && i != num_pipes) {
		pipe(fd[i%2]); // alternative pipe
	}
	pid = fork();
	// child process
	if (pid == 0) {
		// redirect stdin if not NULL or following a pipe
		if (in != NULL) {
			my_fd = open(in, O_RDONLY, 0600);
			dup2(my_fd, STDIN_FILENO);
			close(my_fd);
		} else if (num_pipes > 0 && i != 0) {
			dup2(fd[(i+1)%2][0], STDIN_FILENO);
		}
		// redirect stdout if not NULL or next is a pipe
		if (out != NULL) {
			my_fd = open(out, O_CREAT | O_TRUNC | O_WRONLY, 0600);
			dup2(my_fd, STDOUT_FILENO);
			close(my_fd);
		} else if (num_pipes > 0 && i != num_pipes) {
			dup2(fd[i%2][1], STDOUT_FILENO);
		}
		execvp(cmd, args); // end process
	} else {
		// not backgroud? add to pgid
		if (!bg) {
			if (pgid < 0) pgid = pid;
			setpgid(pid, pgid);
		}
		// free pipe file description
		if (num_pipes != 0) {
			if (i != 0) close(fd[(i+1)%2][0]);
			if (i != num_pipes) close(fd[i%2][1]);
		}
		// not run in backgroud, then has to wait for all
		if (!bg && last) {
			while ((pid = waitpid(-pgid, NULL, 0)) > 0) {
			}
		}
	}
}
void command(int i) {
	int j = 0; // args pointer 
	char *in = NULL, *out = NULL; // init in and out for cur cmd
	cmd = tokens[cur]; // assign cmd pointer
	while (tokens[cur] != NULL &&
		strcmp(tokens[cur], "<") != 0 &&
		strcmp(tokens[cur], ">") != 0 &&
		strcmp(tokens[cur], "|") != 0 &&
		strcmp(tokens[cur], "&") != 0) {
		args[j++] = tokens[cur++]; // detect args
	}
	args[j] = NULL; // info execvp its args is over
	// last cmd
	if (tokens[cur] == NULL) {
		redirect(i, NULL, NULL, 1);
		return;
	}
	// redirect in
	if (strcmp(tokens[cur], "<") == 0) {
		in = tokens[cur+1];
		cur += 2;
		// redirect out
		if (tokens[cur] != NULL && strcmp(tokens[cur], ">") == 0) {
			out = tokens[cur+1];
			cur += 2;
		}
	// redirect out
	} else if (strcmp(tokens[cur], ">") == 0) {
		out = tokens[cur+1];
		cur += 2;
		// redirect in
		if (tokens[cur] != NULL && strcmp(tokens[cur], "<") == 0) {
			in = tokens[cur+1];
			cur += 2;
		}
	}
	// last cmd
	if (tokens[cur] == NULL) {
		redirect(i, in, out, 1);
	// background run, regard as end of cmds
	} else if (strcmp(tokens[cur], "&") == 0) {
		redirect(i, in, out, 1);
	// pipes
	} else if (strcmp(tokens[cur], "|") == 0) {
		redirect(i, in, out, 0);
		cur++;
		// next cmd needed
		command(i+1);
	}
}
// check file or cmd
int fileExist(char *name) {
	int i;
	// file full path
	char file[100];
	// abs path
	if (name[0] == '/' || name[0] == '.') {
		if (access(name, F_OK) == 0) return 0;
		else return 1;
	}
	// built in cmd
	for (i = 0; builtin[i] != NULL; ++i) {
		if (strcmp(name, builtin[i]) == 0) return 0;
	}
	// relative path int env
	for (i = 0; paths[i] != NULL; ++i) {
		strcpy(file, paths[i]);
		strcat(file, "/");
		strcat(file, name);
		if (access(file, F_OK) == 0) return 0;
	}
	return 1;
}
// prev judge gramma
int judge() {
	int i;
	int in_a = -1; // first <
	int out_a = -1; // first >
	int pipe_a = -1; // first |
	int pipe_b = -1; // last |
	// first one is cmd
	if (fileExist(tokens[0]) != 0) {
		printf("%s not exist\n", tokens[0]);
		return 1;
	}
	for (i = 1; tokens[i] != NULL; ++i) {
		if (strcmp(tokens[i], "&") == 0) {
			bg = 1;
			break;
		}
		// check redirect input or output file
		if (strcmp(tokens[i-1], "<") == 0 ||
			strcmp(tokens[i-1], "|") == 0) {
			if (fileExist(tokens[i]) != 0) {
				printf("%s: not exist\n", tokens[i]);
				return 1;
			}
		}
		// set first
		if (strcmp(tokens[i], "<") == 0 && in_a == -1) in_a = i;
		if (strcmp(tokens[i], ">") == 0 && out_a == -1) out_a = i;
		if (strcmp(tokens[i], "|") == 0 && pipe_a == -1) pipe_a = i;
		if (strcmp(tokens[i], "|") == 0) pipe_b = i;
		if (strcmp(tokens[i], "|") == 0) ++num_pipes;
	}
	// <> or >< or < | | | ... > is ok, else not
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
// init shell, env
void initShell() {
	int i = 1;
	char path[MAXCHAR];
	strcpy(path, (const char *)getenv("PATH"));
	if ((paths[0] = strtok(path, ":")) == NULL) return;
	while ((paths[i] = strtok(NULL, ":")) != NULL) ++i;
}
// change directory
void cd(char* path) {
	if (path == NULL) chdir(getenv("HOME")); 
	else{ 
		if (chdir(path) == -1) {
			printf(" %s: no such directory\n", path);
   		}
	}
}
// forever loop until leave
void run() {
	while (1) {
		prompt(); // show cwd
		fgets(line, MAXCHAR, stdin); // get cmd
		// init cmd
		num_tokens = 1; 
		cur = 0;
		pgid = -1;
		bg = 0;
		num_pipes = 0;
		// split cmd, if empty read next line
		if ((tokens[0] = strtok(line, " \t\n")) == NULL) {
			printf("\n");
			continue;
		}
		while ((tokens[num_tokens] = strtok(NULL, " \t\n")) != NULL) {
			++num_tokens;
		}
		// built in function check
		if (strcmp(tokens[0], "leave") == 0) break;
		else if (strcmp(tokens[0], "cd") == 0) cd(tokens[1]);
		else if (strcmp(tokens[0], "clear") == 0) system("clear");
		else {
			// check syntax and analysis cmds
			if (judge() == 0) command(0);
			else printMsg("Error");
		}
	}
}