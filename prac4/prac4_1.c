#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

char *read_quote(int *flag_quote, FILE *file_in);
char *read_symb(char c, FILE *file_in);

// если мы будем считывать строку из файла, то для опредленности будем прописывать -i <file> 
FILE* file_in;

// функция проверки на спец. символ 
int check(int c){
	return c != EOF && c != '\n' && c != ';' && c != '&' && c != ' ' && c != '|' && c != '"' && c != '>' && c != '<' && c != '(' && c != ')';
}

// процедура чтения слова 
char *read_word(int *flag_quote, int *flag_n, FILE *file_in){
    char *str = NULL;
    int i = 0, length = 4;
    char c;

    if (*flag_quote) return read_quote(flag_quote, file_in);

    c = getc(file_in);
    if (c == EOF) return NULL;

    while (c == ' ') c = getc(file_in);

    if (c == '"') return read_quote(flag_quote, file_in);

    // обработка спец. символов
    str = read_symb(c, file_in);
    if (str != NULL) return str;

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
    
    if (c == '\n'){
        *flag_n = 1;
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
        printf("ERROR: open quotation mark \n");
        return NULL;
    }
    else return str;
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

int main(int argc, char *argv[]){
    
    int i, flag_quote = 0, flag_i = 0, length = 8, flag_n = 0;
    char *str, *str_n = "\n";
   
    // анализ командной строки -- чтение со стандартного ввода или из файла?
    for (int i = 1; i < argc; i++){
        if (!strcmp(argv[i], "-i")) flag_i = 1;
    }
    if (flag_i){
        if ((file_in = fopen(argv[2], "r")) == NULL){
            printf("File not found\n");
            return -1;
        }
    } 
    else file_in = stdin;

    str = read_word(&flag_quote, &flag_n, file_in);
    while (str != NULL){
        i = 0;
        char **arr_str = (char **)malloc(length * sizeof(char*));

        while (strcmp(str, str_n) && !flag_n){
            if (i == length - 1){
                length *= 2;
                arr_str = (char **)realloc(arr_str, length * sizeof(char*));
            }
            arr_str[i] = str;
            i++;
            str = read_word(&flag_quote, &flag_n, file_in);
        }
        
        free(str);
        arr_str[i++] = NULL;
        print_arr(arr_str);
        free_arr(arr_str, i);

		str = read_word(&flag_quote, &flag_n, file_in);
    }
    
    fclose(file_in);

    return 0;
}