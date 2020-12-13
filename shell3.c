#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define YELLOW "\033[1;33m"
#define CYAN "\033[0;36m"
#define RESET "\033[0m"

int *spec_sym_arr = NULL;	//there is no other way :((
int spec_sym_idx = 0;
int sym_idx = 0;
int data_idx = 0;
int amperflag = 0;
int *pid_arr = NULL;
int pid_count = 0;
int pid_idx = 1; 

//void print_spec();

char *get_cwd() {	
	static char cwd[PATH_MAX];
	static char *err_msg = "(getcwd() error)";
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		return cwd;
	} else {
		return err_msg;
	}
}

char *get_word() {
	char c;
	if ((c = getchar()) == '\n') {
		return NULL;
	} else if (c == EOF) {
		printf("\n");
		exit(0);
	} else if (c == ' ') {
		return get_word();
	} else {
		ungetc(c, stdin);
	}
	int size = 0, flag = 0;
	char *word = (char *) malloc(sizeof(char));
	c = getchar();
	while ((c != EOF) && (!isspace(c) || flag)) {
		size++;
		word = (char *) realloc(word, sizeof(char) * (size + 1));
		if (c == '\n') {
			printf("Error: отсутствует открывающая/закрывающая кавычка\n");
			free(word);
			return NULL;
		} else if (c == '"') {
			flag = !flag;
			size--;
		} else {
			word[size - 1] = c;
		}
		c = getchar();
	}
	word[size] = '\0';
	if (c == '\n') {
		ungetc(c, stdin);
	}
	return word;
}	

int spec_sym_process(char *word) {	//stage 3+
	/*
		0 - |
		1 - &
		2 - ||
		3 - &&
		4 - ;
	*/
	if (!strcmp(word, "|")) {
		spec_sym_arr = realloc(spec_sym_arr, sizeof(int) * (2 + spec_sym_idx));	//FIXME
		spec_sym_arr[spec_sym_idx] = 0;
		return 1;
	} else if (!strcmp(word, "&")) {
		spec_sym_arr = realloc(spec_sym_arr, sizeof(int) * (2 + spec_sym_idx));
	        spec_sym_arr[spec_sym_idx] = 1;
		return 1;
	/*
	} else if (!strcmp(word, "||")) {
	        spec_sym_array[idx] = 2;
		return 1;
	} else if (!strcmp(word, "&&")) {
        	spec_sym_array[idx] = 3;
		return 1;
	} else if (!strcmp(word, ";")) {
	        spec_sym_array[idx] = 4;
		return 1;
	*/
	} else {
		return 0;
	
	}
}

char ***get_argv() {
	char *word = NULL;
	int i = 0, j = 0;
	char ***data_arr = (char ***) malloc(sizeof(char **));
	spec_sym_arr = (int *) malloc(sizeof(int));
	data_arr[0] = (char **) malloc(sizeof(char *));
	while ((word = get_word()) != NULL) {
		if(!strcmp(word, "&")) {
			amperflag++;
            	} else if(amperflag) {
			amperflag++;
		}
		if (spec_sym_process(word)) {
			free(word);
			data_arr[i][j] = NULL;
			j = 0;
			i++;
			data_arr = realloc(data_arr, sizeof(char **) * (1 + i));
			data_arr[i] = (char **) malloc(sizeof(char *));
			spec_sym_idx++;
			continue;
		}
		data_arr[i][j] = word;
		j++;
		data_arr[i] = realloc(data_arr[i], sizeof(char *) * (1 + j));
	}
	if (i || j) {
		data_arr[i][j] = NULL;
		i++;
		data_arr = realloc(data_arr, sizeof(char **) * (1 + i));
	}
	if (!i) {
		free(data_arr[i]);
	}
	data_arr[i] = NULL;
	spec_sym_arr[spec_sym_idx] = -1;
	return data_arr;
}

void destroy(char ***data_arr) {
	int i = 0;
	int j;
	while (data_arr[i] != NULL) {
		j = 0;
		while (data_arr[i][j] != NULL) {
			free(data_arr[i][j]);
			j++;
		}
		free(data_arr[i]);
		i++;
	}
	free(data_arr);
	return;
}

int custom_funcs(char ***data_arr, int i) {
	int my_func = 0;
	if (!strcmp(data_arr[i][0], "exit")) { 
		free(spec_sym_arr);
		destroy(data_arr);
		free(pid_arr);
		exit(0);
	} else if (!strcmp(data_arr[i][0], "cd")) {
		if (data_arr[i][1]) {
			errno = 0;
			if (chdir(data_arr[i][1]) == -1) {
				perror("chdir error");
			}
		} else {
			chdir(getenv("HOME"));
		}
		my_func = 1;
	}	
	return my_func;
}

int *io_redirect(char **process_args) {
	int i = 1;
	static int fio[2] = {0, 1};
	while (process_args[i] != NULL) {	
		if (!strcmp(process_args[i], ">")) {
			if (process_args[i + 1] != NULL) {
				fio[1] = open(process_args[i + 1], O_WRONLY | O_TRUNC | O_CREAT, 0666);
				if (fio[1] < 0) {
					fio[1] = 1;
					perror("Can't open file: ");
					exit(4);
				}
			}
		} else if (!strcmp(process_args[i], "<")) {
			if (process_args[i + 1] != NULL) {
				fio[0] = open(process_args[i + 1], O_RDONLY);
				if (fio[0] < 0) {
					fio[0] = 0;
					perror("Can't open file: ");
					exit(5);
				}
			}		
		} else if (!strcmp(process_args[i], ">>")) {
			if (process_args[i + 1] != NULL) {
				fio[1] = open(process_args[i + 1], O_WRONLY | O_APPEND | O_CREAT, 0666);
				if (fio[1] < 0) {
					fio[1] = 1;
					perror("Can't open file: ");
					exit(6);
				}			
			}
		} 
		i++;
	}
	return fio;
}

void remove_redirections(char **data_arr) {
	int i = 0;
	while (data_arr[i] && strcmp(data_arr[i], "<") && strcmp(data_arr[i], ">") && strcmp(data_arr[i], ">>")) {
		i++;
	}
	if (data_arr[i]) {
		free(data_arr[i]);
		data_arr[i] = NULL;
		while (data_arr[++i] != NULL) {
			free(data_arr[i]);
		}
	}
	return;
}

void process_argv(char ***data_arr) {
	int pid;
	int run = 1;
	int fd[2];
	int *fio = NULL;
	if ((data_arr[data_idx] != NULL) && (spec_sym_arr[sym_idx] != 0)) {
		if (custom_funcs(data_arr, data_idx)) {
			return;
		}
	}
	if ((pid = fork()) == 0) {
		do {
			pipe(fd);
			switch (fork()) {
				case -1: //WE GOT SOME TROUBLES
				case 0: 
					if (spec_sym_arr[sym_idx] == 0) {
						dup2(fd[1], 1);
					}
					close(fd[0]);
					close(fd[1]);
					fio = io_redirect(data_arr[data_idx]);
					dup2(fio[0], 0);
					dup2(fio[1], 1);
					remove_redirections(data_arr[data_idx]);
					if (fio[0] != 0) {
						close(fio[0]);
					}
					if (fio[1] != 1) {
						close(fio[1]); 
					}
					if (custom_funcs(data_arr, data_idx)) {
						free(pid_arr);
						free(spec_sym_arr);
						destroy(data_arr);
						exit(3);
					}
					else {
						execvp(data_arr[data_idx][0], data_arr[data_idx]);
						perror("non custom command execution error");
						free(spec_sym_arr);
						destroy(data_arr);
						free(pid_arr);
						exit(1);
					}
			}
			dup2(fd[0], 0);
			close(fd[0]);
			close(fd[1]);
			data_idx++;
			if (spec_sym_arr[sym_idx] != 0) {
				run = 0;
			} 
			sym_idx++;
		} while (run);
		while (wait(NULL) != -1) {
			;
		}
		destroy(data_arr);
		free(spec_sym_arr);
		free(pid_arr);
		exit(2);
	} else {
		while (spec_sym_arr[sym_idx] == 0) {
			sym_idx++;
			data_idx++;
		}
		if (spec_sym_arr[sym_idx] != 1) {
			wait(NULL);
		} else {
			pid_arr[0]++;
			pid_arr = realloc(pid_arr, sizeof(int) * (2 + pid_idx));
			pid_arr[pid_idx] = pid;
			pid_arr[pid_idx + 1] = -1;
			sym_idx++;
			pid_idx++;
			printf("[%d] %d\n", pid_arr[0], pid);
		}
		data_idx++;
	}
	return;
}

int get_ended_proc(int pid) {
	int size = pid_arr[0];
	int i = 1;
	while(i <= size)
	{
		if(pid_arr[i] == pid)
		{
			pid_arr[i] = -2;
			return i;
		}
		i++;
	}
	return 0;
}

void check_process(int SIG) {
	signal(SIGCHLD, check_process);
    	int status = 0, pid, i;
    	pid = waitpid(-1, &status, WNOHANG);
    	if(pid > 0) {
        	i = get_ended_proc(pid);
        	if(i > 0) {
            		if(WIFEXITED(status)) { 
				printf("\n[%d]+ Done\n", i);
			} else if (WIFSIGNALED(status)) {
				printf("\n[%d]+ Terminated\n", i);
			}	
			pid_arr[0]--;
            	pid_arr[i] = 0;
        	}
    	}
}

void main_process(char ***data_arr) {
	do {
		//print_spec();
		switch (spec_sym_arr[sym_idx]) {
			case 1:
			case 0:
				process_argv(data_arr);
				break;
			case -1:
				process_argv(data_arr);
				break;
			/*
			case 2:
			case 3:
			case 4:
			*/
		}
	} while (spec_sym_arr[sym_idx] != -1);
}

//TEST START
/*
void print_pid() {
	int i = 0;
	while (pid_arr[i] != -1) {
		printf("%d ", pid_arr[i]);
		i++;
	}
	printf("\n");
}
	

void print_list(char ***data_arr) {
	int i = 0;
	while(data_arr[i] != NULL) {
		int j = 0;
		while (data_arr[i][j] != NULL) {
			printf("%s, ", data_arr[i][j]);
			j++;
		}
		printf("\n");
		i++;
	}

}

void print_spec() {
	int i = 0;
	while (spec_sym_arr[i] != -1) {
		printf("%d ", spec_sym_arr[i]);
		i++;
	}
}
*/
//TEST END
int main() {
	char ***data_arr = NULL;
	pid_arr = (int *) malloc(sizeof(int) * 2);
	pid_arr[0] = 0;	
	pid_arr[1] = -1;
	signal(SIGCHLD, check_process);
	for (;;) {
		printf("%sDum-E:%s%s%s$ ", YELLOW, CYAN, get_cwd(), RESET);
		data_arr = get_argv();
		//print_spec();
		//print_list(data_arr);
		if (amperflag > 1) {
			fprintf(stderr, "Ampersand should be implemented in the END.\n");
		} else {	
			main_process(data_arr);
		} 
		sym_idx = 0;
		data_idx = 0;
		spec_sym_idx = 0;
		//print_pid();
		destroy(data_arr);
		free(spec_sym_arr);
		check_process(SIGCHLD);
		amperflag = 0;
	}
	//KILL CHILD
	free(pid_arr);
	return 0;
}
