#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#define YELLOW "\033[1;33m"
#define CYAN "\033[0;36m"
#define RESET "\033[0m"


char *get_cwd() {	
	static char cwd[PATH_MAX];	//function returns address of local variable
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

char **get_argv(char *word) {
	int i = 0;
	char **data_arr = (char **) malloc(sizeof(char *));
	while ((word = get_word()) != NULL) {
		data_arr[i] = word;
		i++;
		data_arr = realloc(data_arr, sizeof(char *) * (1 + i));
	}
	data_arr[i] = NULL;
	return data_arr;
}

void destroy(char **data_arr) {
	int i = 0;
	while (data_arr[i] != NULL) {
		free(data_arr[i]);
		i++;
	}
	free(data_arr);
	return;
}

int custom_funcs(char **data_arr) {
	int my_func = 0;
	if (!strcmp(data_arr[0], "exit")) { 
		destroy(data_arr);
		my_func = 1;
		exit(0);
	} else if (!strcmp(data_arr[0], "cd")) {
		if (data_arr[1]) {
			errno = 0;
			if (chdir(data_arr[1]) == -1) {
				perror("chdir error");
			}
		} else {
			chdir(getenv("HOME"));	//найти замену chdir("~") (так не работает)	
		}
		my_func = 1;
	}	
	return my_func;
}

void process_argv(char **data_arr) {
	if (data_arr[0] == NULL) {
		;
	} else if (custom_funcs(data_arr)) {
		;
	} else if (fork() == 0) {
		execvp(data_arr[0], data_arr);
		perror("non custom command execution error");
		destroy(data_arr);
		exit(1);
	}
	wait(NULL);
	destroy(data_arr);
	return;
}

int main() {
	char **data_arr = NULL;
	char *word = NULL;
	for (;;) {
		printf("%sDum-E:%s%s%s$ ", YELLOW, CYAN, get_cwd(), RESET);
		data_arr = get_argv(word);
		process_argv(data_arr);
	}
	if (!word) {
		free(word);
	}

	return 0;
}
