#define _GNU_SOURCE 
#define _DEFAULT_SOURCE 1
#define _SVID_SOURCE
#define ARG_l 1
#define ARG_x 2
#define ARG_R 4
#define ARG_S 8
#define ARG_s 16
#define LENGTH 1024

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <errno.h>
#include "limits.h"

#pragma region init
int print_dir(int dirsCount,int args, char * directory);
int recur(int args, char * directory, int *summary);
int get_arg(char * arg, int init);
int is_hidden_file(char *name);
int print_norm(struct stat stats, char *filename, int args);
int print_l(struct stat stats , char *filename, char *path, int args);
int iterate_args(int *dirsC,int argc, char *argv[]);
int get_dir(int* argc, char * directory,char *argv[],int* dirsC);
#pragma endregion

int main(int argc, char *argv[])
{
    int args=0;
    int dirs_Count=0; 
    char directory[LENGTH]={0};
    int summary[6]={0};

    // Получаем аргументы 
    args = iterate_args(&dirs_Count,argc,argv);

    // Проверка аргументов
    if(args == -1){
        printf("Использование: %s [ПАРАМЕТР]… [ФАЙЛ]…\n",argv[0]);
        printf("Аргументы:\n");
        printf("\t-x \t перечислять по строкам, а не по столбцам\n");
        printf("\t-l \t расширенный формат вывода\n");
        printf("\t-R \t рекурсивно показывать каталоги\n");
        printf("\t-S \t сортировать по размеру файлов, сначала большие\n");
        printf("\t-s \t печатать размер каждого файла в блоках\n");
        return 2;
    }
    
    int tmpargc=argc;
    
    int flag;
    if (dirs_Count == 0){
        getcwd(directory,LENGTH);
        flag=1;
	}
    else
        flag=get_dir(&tmpargc,directory,argv,&dirs_Count);

    do{
        if(flag==1){
            // Печать указанной директории
            print_dir(dirs_Count,args, directory);

            // Рекурсия после вывода текущего каталога
            if(args & ARG_R){
                printf("\n%s\n", directory);
                recur(args, directory, summary); 
            }
        }
    }while(flag = get_dir(&tmpargc,directory,argv,&dirs_Count));
    
    printf("\n");
    return 0;
}

/*Сортировка по размеру файла*/
int sortsize(const struct dirent **d1, const struct dirent **d2)
{
    
  	struct stat entry1,entry2;
  	lstat((*d1)->d_name,&entry1);
  	lstat((*d2)->d_name,&entry2);
    
    if(entry1.st_blocks == entry2.st_blocks)
        return (strcasecmp((*d1)->d_name, (*d2)->d_name));
    else 
        return (entry1.st_blocks < entry2.st_blocks);
}

/* Печать текущей директории */
int print_dir(int dirsCount, int args, char * directory){
        
    int n = -1;
    int total_blocks=0;
    struct dirent **namelist;
    struct stat stat_struct;
    char pwd[LENGTH]={0};
    getcwd(pwd,LENGTH);

    if ((args & ARG_R)|dirsCount>1)
        printf("\n\n%s:\n",directory);
    char dirdup[LENGTH];
    strncpy(dirdup,directory,2);
    if (!strcmp(dirdup,"..") || !strcmp(dirdup,"./"))
    {
        char doubleDot[LENGTH];
        getcwd(doubleDot,LENGTH);
        strcat(doubleDot,"/");
        strcat(doubleDot,directory);
        directory=strdup(doubleDot);
    }
    
    chdir(directory);

    if (args & ARG_S)
        n=scandir(directory, &namelist, NULL, sortsize); //сортировка по размеру блоков
    else 
        n=scandir(directory, &namelist, NULL, alphasort ); //алфавитная сортировка
    //closedir(dir);
    chdir(pwd);
    
    if (n < 0) // вернулся код ошибки, пробуем искать
    {

        // если 1 файл делаем lstat
        if(lstat(directory, &stat_struct)==-1)
        {

            // Не файл и не директория
            //printf("Error in print_dir on file: [%s]\n", directory);
            printf("%s\n", strerror(errno));
            return 1;
        }
        else
        {
            // 1 файл
            printf("%lu\t%s\n ", stat_struct.st_blocks/2, directory);
        }
    }
    else
    {
        // Цикл по файлам
        int num=-1;
        while(++num < n)
        {

            // Если  файл не спрятан
            if(!is_hidden_file(namelist[num]->d_name)){

                // Проверка файла
                char file[LENGTH*4];
                strcpy(file, directory);

                // Не добавляем, если /
                if(strcmp("/", directory))
                    strcat(file, "/");
                strcat(file, namelist[num]->d_name);
                strcat(file, "\0");

                // error
                if(lstat(file, &stat_struct)==-1){
                    printf("Error in print_dir on file: [%s]\n", file);
                    printf("%s\n", strerror(errno));
                    return 1;
                }

                // Если флаг -l и не флаг -x
                if((args & ARG_l) & !(args & ARG_x)){
                    print_l(stat_struct, namelist[num]->d_name, directory,args);
                    total_blocks += stat_struct.st_blocks;

                }

                // нормальная печать 
                else{
                    print_norm(stat_struct, namelist[num]->d_name,args);
                    total_blocks += stat_struct.st_blocks;
                    
                }
            }
            // Освобождение памяти в куче
            free(namelist[num]);
        }
        free(namelist);
    }
    if (args & ARG_s)
        printf("\nитого: %d\n",total_blocks/2);
    else
        printf("\n");
    return 0;
}

/* Рекурсивный вход в директории */
int recur(int args, char * directory, int *summary){

    int len = sizeof(directory) / sizeof(directory[0]);
    int n = -1;
    int num=-1;
    struct dirent **namelist;
    struct stat stat_struct;
    n=scandir(directory, &namelist, NULL, alphasort); 
    if (n < 0){

        // если 1 файл делаем lstat
        if(lstat(directory, &stat_struct)==-1)
        {

            // Не файл и не директория
            printf("Error in print_dir on file: [%s]\n", directory);
            printf("%s\n", strerror(errno));
            return 1;
        }
        else
        {
            // 1 файл
            printf("%lu\t%s\n ", stat_struct.st_blocks, directory);
        }
    }

    // Цикл по файлам
    while(++num < n){
        if(!(strcmp(".", namelist[num]->d_name) && strcmp("..",namelist[num]->d_name) && !is_hidden_file(namelist[num]->d_name))){ 
            free(namelist[num]);
            continue;
        }
        char file[LENGTH*4];
        strcpy(file, directory);


        // Не добавлять если /
        if(strcmp("/", directory))
            strcat(file, "/");
        strcat(file, namelist[num]->d_name);
        strcat(file, "\0");

        // Проверка файла
        if(lstat(file, &stat_struct)==-1){
            printf("Error in recur on file: [%s]\n", file);
            printf("%s\n", strerror(errno));
            free(namelist[num]);
            return 1;
        }
        
        // Если каталог, рекурсируем
        if(S_ISDIR(stat_struct.st_mode)){
            print_dir(4,args, file);

            recur(args, file, summary);
        }
        free(namelist[num]);
    }
    free(namelist);
    return 0;
}

/* возвращает 1, если файл спрятан */
int is_hidden_file(char *name){

	// Спрятаные файлы начинаются с . 
	return name[0] == '.' ? 1:0;
}

/* без ключа -l*/
int print_norm(struct stat stats, char *filename,int args){

    if (args & ARG_s)
        printf("%5ld  %s\t",stats.st_blocks/2, filename); 
    else {
        printf("%s  ", filename);
    }
    
	return 0;
}

/* ключ -l*/
int print_l(struct stat stats, char *filename, char * path,int args){

    // Mode
    mode_t mode = stats.st_mode;
    char perm[64]={0};
    char slink[64] = {'\0'};

    // File Type
    switch(mode & S_IFMT){
        case S_IFCHR:
            strcat(perm, "c");
            break;
        case S_IFBLK:
            strcat(perm, "b");
            break;
        case S_IFIFO:
            strcat(perm, "p");
            break;
        case S_IFDIR:
            strcat(perm, "d");
            break;
        case S_IFLNK:
            strcat(perm, "l");
            break;
        case S_IFSOCK:
            strcat(perm, "s");
            break;
        case S_IFREG:
            strcat(perm, "-");
            break;
        default:
            strcat(perm, "?");
            break;
    }

    mode_t  m[9] = {
        S_IRUSR, S_IWUSR, S_IXUSR, 
        S_IRGRP, S_IWGRP, S_IXGRP,
        S_IROTH, S_IWOTH, S_IXOTH
        };
    for(int i=0; i<9; i++){
        switch(m[i] & mode){
            case S_IRUSR:
            case S_IRGRP:
            case S_IROTH:
                strcat(perm, "r");
                break;
            case S_IWUSR:
            case S_IWGRP:
            case S_IWOTH:
                strcat(perm, "w");
                break;
            case S_IXUSR:
            case S_IXGRP:
            case S_IXOTH:
                strcat(perm, "x");
                break;
            default:
                strcat(perm, "-");
                break;
        }
    }

    // SUID 
    if(mode & S_ISUID)
        perm[3] = (mode & S_IXUSR) ? 's' : 'S';
    // GUID 
    if(mode & S_ISGID)
        perm[6] = (mode & S_IXGRP) ? 's' : 'i';
    // Sticky bit   
    if(mode & S_ISVTX)
        perm[9] = (mode & S_IXOTH) ? 't' : 'T';

    // User
    struct passwd *usr = getpwuid(stats.st_uid);
     
    // Group
    struct group * grp = getgrgid(stats.st_gid);

    
    // Last Modified Time
    const double YEAR = 365*24*3600;
    struct tm *time_struct;
    time_t now, modified = stats.st_mtime;
    char buf[80];
    time(&now);
    time_struct = localtime(&modified);
    double time_diff = difftime(now, modified); 
    char format[15] = " %b %d ";

    // Отображать файлы старше года, используя год, а не время
    if(time_diff > YEAR)
        strcat(format,  " %Y");
    else
        strcat(format,  "%R");

    strftime(buf, sizeof(buf), format, time_struct);

    //Символические ссылки
    if(perm[0] == 'l')
        readlink(path, slink, 64);
    char arrow[6]={' '};
    
    if(slink[0] != '\0')
        strcat(arrow, " -> ");

    if (args & ARG_s)
        printf("%ld %s %3d %s %s %7ld %s %s %s %s\n", stats.st_blocks/2, perm, stats.st_nlink, usr->pw_name, grp->gr_name, stats.st_size, buf, filename, arrow, slink); 
    else{ 
	    printf("%s %3d %s %s %7ld %s %s %s %s\n", perm, stats.st_nlink, usr->pw_name, grp->gr_name, stats.st_size, buf, filename, arrow, slink); 
    }
    return 0;
}

/*Счёт древних шизов*/ 
int iterate_args(int *dirsC,int argc, char *argv[]){
	
	if(argc==1) return 0;
	
	// Если ввели только директорию, считаем папку, иначе берём аргумент
	
	if(argv[argc-1][0] != '-'){
		(*dirsC)++;
		--argc;
		return iterate_args(dirsC,argc, argv);
	}
	else{
		--argc;
		return iterate_args(dirsC, argc, argv) | get_arg(argv[argc], 1);
	}
}	

/*Возвраты: 0 - директорий нет; 1 - найдена директория; 2 найден ключ;*/ 
int get_dir(int *argc,char * directory,char *argv[],int* dirsC){
	
	(*argc)--;
	if ((*argc) <= 0){
		return 0;
	}

	if(argv[*argc][0] != '-'){
		strcpy(directory, argv[*argc]);
		return 1;		
	}
	else
		return 2;
}

/*Шифратор аргумента из текста в число*/
int get_arg(char * arg, int init){

	int args=0;

	if(!arg[init]) return 0;

	if(arg[init]=='l'){
		args |= ARG_l;
	}
	else if(arg[init]=='x'){
		args |= ARG_x;
	}
	else if(arg[init]=='R'){
		args |= ARG_R;
	}
	else if(arg[init]=='S'){
		args |= ARG_S;
	}
	else if(arg[init]=='s'){
		args |= ARG_s;
	}
	else
	{
        return -1;
    }
	return args | get_arg(arg, ++init);
}