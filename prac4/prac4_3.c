#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>


struct cmd{                  
	char ** cmdarr;
	int cmdcount;
	struct cmd * cmdnext;
	struct cmd * cmdnextseq;
	struct cmd * cmdbracket;
	char logicflag;
	int bgdflag;
	char* infd;
	char* outfd;
	char* addfd;
};

//инициализиция нашей вспомогательной структуры 
void initcmd(struct cmd * shellstruct)
{
	shellstruct->cmdarr = NULL;
	shellstruct->cmdcount = 0;
	shellstruct->cmdnext = NULL;
	shellstruct->cmdbracket = NULL;
	shellstruct->cmdnextseq = NULL;
	shellstruct->bgdflag = 0;
	shellstruct->infd = NULL;
	shellstruct->outfd = NULL;
	shellstruct->addfd = NULL;
}


char *read_quote(int *flag_quote, FILE *file_in);
char *read_symb(char c, FILE *file_in);

/*
arrays of PIDs and their additional parameters
*/
int * bgdpids = NULL;
int * fgdpids = NULL;

int bgdlen = 0;
int fgdlen = 0;
int lastpid, laststatus;


/*
//функция перевода arr в arr_cmd
char ***arr_to_arr_cmd(char **arr, int len_arr){
    char ***arr_cmd = NULL;
    int length = 8, i = 0, j = 0;
    arr_cmd = (char ***)malloc(len_arr * sizeof(**arr));
    
    printf("conv starting\n");
    
    while (arr[i] != NULL){
        printf("!\n");
        arr_cmd[j] = (char **)realloc(arr_cmd, length * sizeof(char **));
        while (strcmp(arr[i], "|") && strcmp(arr[i], "\n")){
            arr_cmd[j][i] = (char *)malloc(strlen(arr[i]) * sizeof(char *));
            arr_cmd[j][i] = arr[i];
            printf("&&\n");
            i++;
        }
        j++;
        length = length * 2; 
    }
    
    printf("conv ending\n");

    return arr_cmd;
}
*/

// функция проверки на спец символ 
int check(int c){
	return c != EOF && c != '\n' && c != ';' && c != '&' && c != ' ' && c != '|' && c != '"' && c != '>' && c != '<' && c != '(' && c != ')';
}

//  процедура чтения слова 
char *read_word(int *flag_quote, FILE *file_in){
    char *str = NULL;
    int i = 0, length = 4;
    char c;

    if (*flag_quote){
        return read_quote(flag_quote, file_in);
    }
    
    c = getc(file_in);
    
    if (c == EOF){
        return NULL;
        exit(0);
    }

    while (c == ' ') c = getc(file_in);

    if (c == '"'){
        return read_quote(flag_quote, file_in);
    }

    // обработка спец. символов
    str = read_symb(c, file_in);
    if (str != NULL){
        return str;
    }

    str = (char *)malloc(length * sizeof(char));
    str[0] = 0;
    while (check(c)){
        if (i == length - 1)
		{
			length *= 2;
			str = (char *)realloc(str, length * sizeof(char));
		}
		str[i] = c;
		i++;
		str[i] = 0;
		c = getc(file_in);
    }

    if (c != '"'){
        ungetc(c, file_in);
    }
    else *flag_quote = 1;

    return str;
}

char *signs[] = {"","<", ">", ">>", "&", "&&","|", "||", ";", "(", ")","\n"};
enum flags{EMPTY, INPUT, OUTPUT, APPEND, BACKGROUND, AND, PIPE, OR, SEMICOLON, LBRACKET, RBRACKET, RETURN};

// вспомогательная процедура 
char *help_read_symb(enum flags symb){
	char *str;
	str = (char *)malloc((strlen(signs[symb]) + 1) * sizeof(char));
	strcpy(str, signs[symb]);

    return str;
}

// процедура отслеживания спец. символов 
char *read_symb(char c, FILE *file_in){
    switch(c){
        case '\n':
            return help_read_symb(RETURN);
            break;
        case '&':
            c = getc(file_in);
            if(c == '&'){
                return help_read_symb(AND);
            }
            else{
                ungetc(c, file_in);
                return help_read_symb(BACKGROUND);
            }
            break;
        case '|':
            c = getc(file_in);
            if(c == '|'){
                return help_read_symb(OR);
            }
            else{
                ungetc(c, file_in);
                return help_read_symb(PIPE);
            }
            break;
        case '>': 
            c = getc(file_in);
            if(c == '>'){
                return help_read_symb(APPEND);
            }
            else{
                ungetc(c, file_in);
                return help_read_symb(OUTPUT);
            }
            break;
        case '<':
            return help_read_symb(INPUT);
            break;
        case ';':
            return help_read_symb(SEMICOLON);
            break;
        case '(':
            return help_read_symb(LBRACKET);
            break;
        case ')':
            return help_read_symb(RBRACKET);
            break;
        default:
            return NULL;
            break;
    }
}

// случай обработки слова при кавычках 
char *read_quote(int *flag_quote, FILE *file_in){
    int i = 0, length = 4;
    char c;
    char *str = (char *)malloc(length * sizeof(char));
    str[0] = 0;
    c = getc(file_in);

    while (c != '"' && c != '\n'){
        if (i == length - 1){
            length *= 2;
            str = (char *)realloc(str, length * sizeof(char));
        }
        str[i] = c;
        i++;
        str[i] = 0;
        c = getc(file_in);
    }

    *flag_quote = 0;

    if (c == '\n'){
        free(str);
        fprintf(stderr, "Error: Too many arguments or No such file or directory\n");
        return NULL;
    }
    else return str;
}

enum flags symbolcheck (char * symbol){
	enum flags i;

	for(i = EMPTY + 1; i <= RBRACKET ; i++){
		if(strcmp(symbol, signs[i]) == 0){
			return i;
		}
	}

	return EMPTY;
}

// анализ строки
struct cmd *cmdanalyse(char ***cmdarr)
{
	struct cmd *shellstruct = (struct cmd *)malloc(sizeof(*shellstruct)), *first = shellstruct;
	initcmd(shellstruct);
	int situation;
	while (**cmdarr != NULL){
		situation = symbolcheck(**cmdarr);
		switch (situation){
			case EMPTY:  
				shellstruct->cmdcount++;
				shellstruct->cmdarr = (char**)realloc(shellstruct->cmdarr, sizeof(char**)*(shellstruct->cmdcount + 1));
				shellstruct->cmdarr[shellstruct->cmdcount] = NULL;
				shellstruct->cmdarr[shellstruct->cmdcount - 1] = **cmdarr;
				**cmdarr = NULL;
				(*cmdarr)++;
				break;
			case INPUT:
				(*cmdarr)++;
				shellstruct->infd = **cmdarr;
				**cmdarr = NULL;
				(*cmdarr)++;
				break;
			case OUTPUT:
				(*cmdarr)++;
				shellstruct->outfd = **cmdarr;
				**cmdarr = NULL;
				(*cmdarr)++;
				break;
			case APPEND:
				(*cmdarr)++;
				shellstruct->addfd = **cmdarr;
				**cmdarr = NULL;
				(*cmdarr)++;
				break;
			case PIPE:
				shellstruct->cmdnext = malloc(sizeof(*shellstruct));
				shellstruct = shellstruct->cmdnext;
				initcmd(shellstruct);
				(*cmdarr)++;
				break;
			default:
				break;    
		}
	}
	return first;
}

int cmdexecute(struct cmd* shellstruct){
	int i = 0;
	int file, status;
	int fd[2];
	struct cmd *first = shellstruct;
	while (shellstruct != NULL){
		int save0 = dup(0);
		int save1 = dup(1);         
		if(shellstruct->cmdarr != NULL && strcmp(shellstruct->cmdarr[0], "cd") == 0){
			int rez;
			if(shellstruct->cmdarr[1] == NULL){
				if(chdir(getenv("HOME")) == -1){
					printf("Error: you are homeless\n");
					exit(1);
				}
			}
			else{
				rez = chdir(shellstruct->cmdarr[1]);
				if(rez == -1){
					perror(shellstruct->cmdarr[1]);
				}
			}
		}
		else if(shellstruct->cmdarr != NULL && strcmp(shellstruct->cmdarr[0], "exit") == 0){
			exit(0);
		}
		else{
			if(i != 0){
				dup2(fd[0],0);
				close(fd[0]);
			}
			if(shellstruct->cmdnext != NULL){
				pipe(fd);
				dup2(fd[1],1);
				close(fd[1]);
			}
			int pid = fork();
			if(pid == 0){
				if(shellstruct->cmdnext != NULL){
					close(fd[0]);
				}
				else{
					if(shellstruct->infd != NULL){
						file = open(shellstruct->infd, O_RDONLY, 0666);
						if(file == -1){
							perror(shellstruct->infd);
						}
						dup2 (file,0);
						close (file);
					}
				}
				if(shellstruct->outfd != NULL){
					file = open(shellstruct->outfd, O_CREAT | O_WRONLY | O_TRUNC, 0666);
					if (file == -1){
						perror(shellstruct->outfd);
					}
					dup2 (file,1);
					close (file);
				}
				else if (shellstruct->addfd != NULL){
						file = open (shellstruct->addfd, O_APPEND | O_WRONLY, 0666);
						if (file == -1){
							perror(shellstruct->addfd);
						}
						dup2 (file,1);
						close (file);
					}
				else{
					signal(SIGCHLD, SIG_DFL);
					execvp(shellstruct->cmdarr[0],shellstruct->cmdarr);
					perror(shellstruct->cmdarr[0]);
					exit(1);
				}
			}
		}
		
		shellstruct = shellstruct->cmdnext;
		i++;
		dup2(save0,0);
		dup2(save1,1);
		close(save0);
		close(save1);
	}

	if(first->cmdnextseq){
		status = cmdexecute(first->cmdnextseq);  
	}
	return status;
}

// процедура вывода массива
void print_arr(char **arr){
    if (arr == NULL) return;

    int i = 0;
    while (arr[i] != NULL){
        printf("%s\n", arr[i]);
        i++;
    }
}

// процедура освобождения памяти 
void free_arr(char **arr, int i){
    for (int j = 0; j < i; j++) free(arr[j]);
    free(arr);
}

void free_cmd(struct cmd *shellstruct){
	struct cmd *temp;
	while(shellstruct != NULL){
		free(shellstruct->infd);
		free(shellstruct->outfd);
		free(shellstruct->addfd);
		free(shellstruct->cmdarr);
		temp = shellstruct;
		shellstruct = shellstruct->cmdnext;
		free(temp);
	}
}

int main(int argc, char *argv[]){

    int i, flag_quote = 0, length = 8;
    char *str, *str_n = "\n";
    FILE *file_in;
	struct cmd * cmdpointer;

    file_in = stdin;
    // анализ командной строки -- чтение со стандартного ввода или из файла?
    if (argc > 1){
        file_in = fopen(argv[1], "r");
    }
    str = read_word(&flag_quote, file_in);
    
    while (str != NULL){
        i = 0;
        char **arr_str = (char **)malloc(length * sizeof(char*));

        while ((str != NULL) && strcmp(str, str_n)){
            if (i == length - 1){
                length *= 2;
                arr_str = (char **)realloc(arr_str, length * sizeof(char*));
            }
            arr_str[i] = str;
            i++;
            str = read_word(&flag_quote, file_in);
        }
        free(str);
        arr_str[i] = NULL;
        char ** tmparr = arr_str;
        cmdpointer = cmdanalyse(&tmparr);

        cmdexecute(cmdpointer);
        
        

        free_cmd(cmdpointer);
		free_arr(arr_str,i);
		str = read_word(&flag_quote, file_in);
    }

    fclose(file_in);
    
    return 0;
}
