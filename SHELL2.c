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

char ***get_argv() {
	char *word = NULL;
	int i = 0, j = 0;
	char ***data_arr = (char ***) malloc(sizeof(char **));
	data_arr[0] = (char **) malloc(sizeof(char *));
	while ((word = get_word()) != NULL) {
		if (*word == '|') {
			free(word);
			data_arr[i][j] = NULL;
			j = 0;
			i++;
			data_arr = realloc(data_arr, sizeof(char **) * (1 + i));
			data_arr[i] = (char **) malloc(sizeof(char *)); 
			continue;
		}
		data_arr[i][j] = word;
		j++;
		data_arr[i] = realloc(data_arr[i], sizeof(char *) * (1 + j));
	}
	if (i || j) {	//for correct NULL data_arr processing(???)
		data_arr[i][j] = NULL;	//to add NULL for last arg
		i++;
		data_arr = realloc(data_arr, sizeof(char **) * (1 + i));
	}
	if (!i) {
		free(data_arr[i]);
	}
	data_arr[i] = NULL;
	return data_arr;
}

void destroy(char ***data_arr) {
	int i = 0;
	while (data_arr[i] != NULL) {
		int j = 0;
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

int custom_funcs(char ***data_arr) {
	int my_func = 0;
	if (!strcmp(data_arr[0][0], "exit")) { 
		destroy(data_arr);
		my_func = 1;
		exit(0);
	} else if (!strcmp(data_arr[0][0], "cd")) {
		if (data_arr[0][1]) {
			errno = 0;
			if (chdir(data_arr[0][1]) == -1) {
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
	int i = 1;	//i = 0 is our process
	static int fio[2] = {0, 1};
	while (process_args[i] != NULL) {	
		if (!strcmp(process_args[i], ">")) {	//FIXME same value recounting x999 times
			if (process_args[i + 1] != NULL) {	//TODO PULL OUT IF COND
				fio[1] = open(process_args[i + 1], O_WRONLY | O_TRUNC | O_CREAT , 0666);
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
				fio[1] = open(process_args[i + 1], O_WRONLY | O_APPEND | O_CREAT     , 0666);
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
	int fd[2];
	int *fio = NULL;
	int i = 0;
	if ((data_arr[0] != NULL) && (data_arr[1] == NULL)) {
		if (custom_funcs(data_arr)) {
			return;
		}
	}
	if (fork() == 0) {
		while (data_arr[i] != NULL) {
			pipe(fd);
			switch (fork()) {
				case -1: //WE GOT SOME TROUBLES
				case 0: 
					if (data_arr[i + 1] != NULL) {
						dup2(fd[1], 1);
					}
					close(fd[0]);
					close(fd[1]);
					//TODO 2 дескр 1 ввод 2вывод
					fio = io_redirect(data_arr[i]);
					dup2(fio[0], 0);
					dup2(fio[1], 1);
					remove_redirections(data_arr[i]);
					if (fio[0] != 0) {
						close(fio[0]);	//TODO add condition? or nah
					}
					if (fio[1] != 1) {
						close(fio[1]); 
					}
					if (custom_funcs(data_arr)) {
						exit(3);
					}
					else {
						execvp(data_arr[i][0], data_arr[i]);
						perror("non custom command execution error");
						destroy(data_arr);
						exit(1);
					}
			}
			dup2(fd[0], 0);
			close(fd[0]);
			close(fd[1]);
			i++;
		}
		while (wait(NULL) != -1) {
			;
		}
		destroy(data_arr);
		exit(2);
	} else {
		wait(NULL);
	}
	return;



}

//TEST
/*
void print_list(char ***data_arr) {
	int i = 0;
	while (data_arr[i] != NULL) {
		int j = 0;
		while (data_arr[i][j] != NULL) {
			printf("%s, ", data_arr[i][j]);
			j++;
		}
		printf("\n");
		i++;
	}
}
*/
//TEST
int main() {
	char ***data_arr = NULL;
	for (;;) {
		printf("%sDum-E:%s%s%s$ ", YELLOW, CYAN, get_cwd(), RESET);
		data_arr = get_argv();
		process_argv(data_arr);
		destroy(data_arr);
	}

	return 0;
}
