#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

void zmbcheck();
void chldhandler();
int checkpid(int **arrpid, int *len, int pid);
char *read_quote(int *flag_quote, FILE *file_in);
char *read_symb(char c, FILE *file_in);

struct cmd{                  
	char **cmdarr;
	int cmdcount;
	struct cmd *cmdnext;	// 
	struct cmd *cmdnextseq;
	char logicflag; 
	int bgdflag;
	char *infd;
	char *outfd;
	char *addfd;
};

//инициализиция нашей вспомогательной структуры 
void initcmd(struct cmd *shellstruct){
	shellstruct->cmdarr = NULL;
	shellstruct->cmdcount = 0;
	shellstruct->cmdnext = NULL;
	shellstruct->cmdnextseq = NULL;
	shellstruct->bgdflag = 0;
	shellstruct->infd = NULL;
	shellstruct->outfd = NULL;
	shellstruct->addfd = NULL;
}

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

char *signs[] = {"","<", "> ", ">>", "&", "&&","|", "||", ";", "(", ")","\n"};
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

// процедура вывода массива
void print_arr(char **arr){
    if (arr == NULL) return;

    int i = 0;
    while (arr[i] != NULL){
        printf("%s\n", arr[i]);
        i++;
    }
}

/*
массивы pid'ов
*/
int *bgdpids = NULL;
int *fgdpids = NULL;

int bgdlen = 0;
int fgdlen = 0;
int lastpid, laststatus;

int *addpid(int *pointer, int *length, int add){   
    int *result = pointer;
    result = realloc (pointer, (1 + *length) * sizeof(int));
    result [*length]= add;
    (*length)++;
	
    return result;
}

/*
сообщает когда ребенок завершился 
*/
void chldhandler(int status){
	zmbcheck();
}

void zmbcheck(){ 
	int status;
	int pid;
	int flag;
	do{
		pid = waitpid(-1, &status, WNOHANG);   
		if (pid > 0){
			flag = checkpid(&bgdpids, &bgdlen, pid);
			if (flag){
				if (WIFEXITED(status)){
					printf("[%d] exited; status = %d\n", pid, WEXITSTATUS (status));
				}
				else if (WIFSIGNALED (status)){
					printf("[%d] killed by signal %d\n", pid, WTERMSIG (status)); 
				}
				else if (WIFSTOPPED(status)){
					printf("[%d] stopped by signal %d\n", pid, WSTOPSIG(status));
				}
				else if (WIFCONTINUED(status)){
					printf("[%d] continued\n", pid);
				}
			}
			else{
				flag = checkpid(&fgdpids, &fgdlen, pid); 
				if (flag){
					if (lastpid == pid){
						laststatus = status; 
					}
				}
			}
		}
	}
	while(pid > 0);
}

/*
checkpid - выполняет поиск заданного pid в массиве pid'ов
*len - длина массива
если мы нашли наш pid то мы его должны удалить из массива 
0, если pid не найден
1, если найден и удален
*/
int checkpid(int **arrpid, int *len, int pid){  
	int index = -1;
	int j = *len, i;
	for(i = 0; i < j; i++){
		if((*arrpid)[i] == pid){
			index = i;
			break;
		}
	}
	if (index >= 0){
		j--;
		for(i = index; i < j; i++){
			(*arrpid)[i] = (*arrpid)[i+1];
		}
		*len = j;
		return 1;
	}

	return 0;
}

// анализ строки
struct cmd *cmd_analyse(char ***cmdarr){
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
			case BACKGROUND:
				(*cmdarr)++;
				shellstruct->bgdflag = 1;
				if(**cmdarr != NULL){
					printf("Don't have commands to execute\n");
					return first;
				}
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

int cmd_execute(struct cmd *shellstruct){
	int i = 0;
	int file, status;
	int fd[2];
	struct cmd *first = shellstruct;
	
	while (shellstruct != NULL)	{
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
				signal(SIGINT, SIG_DFL);
				if(shellstruct->bgdflag != 0){
					file = open ("/dev/null", O_RDONLY);
					if(file == -1){
						perror(shellstruct->infd);
					}
					dup2 (file,0);
					close (file);
				}
				else{
					if(shellstruct->infd != NULL){
						file = open(shellstruct->infd, O_RDONLY);
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
				else{
					if (shellstruct->addfd != NULL){
						file = open (shellstruct->addfd, O_APPEND | O_WRONLY);
						if (file == -1){
							perror(shellstruct->addfd);
						}
						dup2 (file,1);
						close (file);
					}
				}

                signal(SIGCHLD, SIG_DFL);
                execvp(shellstruct->cmdarr[0],shellstruct->cmdarr);
                perror(shellstruct->cmdarr[0]);
                exit(1);
			}
			else{
				if(shellstruct->bgdflag){
					printf("child pid: [%d]\n", pid);
					bgdpids = addpid(bgdpids, &bgdlen, pid);
				}
				else{
					fgdpids = addpid(fgdpids, &fgdlen, pid);
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
	
	status = laststatus;
	if(first->cmdnextseq){
		status = cmd_execute(first->cmdnextseq);  
	}

	return status;
}
 
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
	struct cmd *cmdpointer;

    file_in = stdin;
    // анализ командной строки -- чтение со стандартного ввода или из файла?
    if (argc > 1){
        file_in = fopen(argv[1], "r");
    }

    signal (SIGCHLD, chldhandler);
    printf("> ");
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
        char **tmparr = arr_str;

        cmdpointer = cmd_analyse(&tmparr);
        
        cmd_execute(cmdpointer);

        free_cmd(cmdpointer);
		free_arr(arr_str,i);
        printf("> ");
        str = read_word(&flag_quote, file_in);
    }

    fclose(file_in);
	    
    return 0;
}
