#ifndef SYNC_H
#define SYNC_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <openssl/md5.h>

#define MAX_PATHLENGTH 4096
#define MAX_FILENAME 255

// 로그 파일(monitor_list.log)의 내용을 메모리에 담아둘 연결 리스트 노드
typedef struct log_node {
    char *log_data;
    struct log_node *next;
} LogNode;

// 데몬 프로세스가 모니터링할 타겟 파일들의 정보를 담는 연결 리스트 노드
typedef struct target_node {
    char path[MAX_PATHLENGTH];
    int mtime;
    bool is_removed; 
    unsigned char *hash;
    struct target_node *next;
} TargetNode;

/* 전역 변수 외부 선언 (main.c에서 실제 정의) */
extern char home_dir[MAX_PATHLENGTH];
extern char backup_dir[MAX_PATHLENGTH + 16];
extern int monitor_list_fd;
extern LogNode *log_list_head;

/* utils.c */
void init_environment(void);
void load_monitor_list(void);
void append_log_node(char *log_data);
void add_daemon_to_list(pid_t pid, char *path);
void sync_monitor_list_file(void);
void remove_log_node(LogNode *target);
LogNode *find_log_node_by_pid(char *pid_str);
int parse_command(char *command);
char *get_absolute_path(char *path);
bool is_valid_path(char *path);
void append_to_monitor_list_file(pid_t pid, char *path);

/* help.c */
void print_usage(int command, int flag);
void print_usage_all(void);

/* 각 명령어 파일의 메인 진입점 */
void cmd_add(int argc, char *argv[]);
void cmd_remove(int argc, char *argv[]);
void cmd_list(int argc, char *argv[]);

#endif
