#include "sync.h"

/*
 * 로그 문자열을 받아 새로운 노드를 생성하고 전역 로그 연결 리스트(log_list_head)의 끝에 추가한다.
 * char *log_data: 추가할 로그 문자열 (예: "PID : PATH")
 */
void append_log_node(char *log_data) {
    LogNode *tmp = (LogNode *)malloc(sizeof(LogNode)); 
    tmp->log_data = strdup(log_data);
    tmp->next = NULL;
    
    if (log_list_head == NULL) {
        log_list_head = tmp;
    } else {
        LogNode *curr = log_list_head;
        while (1) {
            if (curr->next == NULL) {
                curr->next = tmp;
                break;
            }
            curr = curr->next;
        }
    }
}

/*
 * 데몬 프로세스의 PID와 경로를 문자열로 합쳐 로그 리스트에 추가한다.
 * pid_t pid: 데몬 프로세스의 PID
 * char *path: 모니터링 대상 절대 경로
 */
void add_daemon_to_list(pid_t pid, char *path) {
    char buf[5000];
    char pid_buf[16];
    sprintf(pid_buf, "%d", pid);
    strcpy(buf, pid_buf);
    strcat(buf, " : ");
    strcat(buf, path);
    append_log_node(buf);
}

/*
 * 백업 디렉토리의 monitor_list.log 파일을 읽어들여 로그 연결 리스트를 초기화한다.
 */
void load_monitor_list(void) {
    char c;
    char buf[5000];
    int i = 0;
    
    lseek(monitor_list_fd, 0, SEEK_SET);
    while (read(monitor_list_fd, &c, 1) > 0) {
        buf[i++] = c;
        if (c == '\n' || c == EOF) {
            buf[i - 1] = '\0';
            append_log_node(buf);
            memset(buf, 0, sizeof(buf));
            i = 0;
        }
    }
}

/*
 * 로그 링크드 리스트에서 특정 노드를 찾아 연결을 끊고 메모리를 해제한다.
 * LogNode *target: 삭제할 대상 노드 포인터
 */
void remove_log_node(LogNode *target) {
    if (target == NULL || log_list_head == NULL) return;
    
    LogNode *tmp = log_list_head;
    if (target == log_list_head) { 
        log_list_head = log_list_head->next;
        free(target->log_data);
        free(target);
        return;
    }
    
    while (tmp != NULL) {
        if (tmp->next == target) break;
        tmp = tmp->next;
    }
    
    if (tmp == NULL) return;
    
    tmp->next = target->next;
    free(target->log_data);
    free(target);
}

/*
 * 메모리 상의 로그 연결 리스트를 기반으로 monitor_list.log 파일을 새롭게 덮어쓴다.
 */
void sync_monitor_list_file(void) {
    char log_path[MAX_PATHLENGTH * 2];
    snprintf(log_path, sizeof(log_path), "%s/monitor_list.log", backup_dir);
    
    close(monitor_list_fd);
    monitor_list_fd = open(log_path, O_RDWR | O_CREAT | O_TRUNC | O_APPEND | O_SYNC, 0666);
    
    LogNode *tmp = log_list_head;
    while (tmp != NULL) {
        if (write(monitor_list_fd, tmp->log_data, strlen(tmp->log_data)) < 0){
            fprintf(stderr, "unable to write log\n");
        }
        if (write(monitor_list_fd, "\n", 1) < 0){
            fprintf(stderr, "unable to write log\n");
        }
        tmp = tmp->next;
    }
}

/*
 * PID 문자열을 이용해 로그 링크드 리스트에서 특정 노드를 찾아 반환한다.
 * char *pid_str: 찾을 데몬 프로세스의 PID 문자열
 * return : LogNode*, 일치하는 PID를 가진 노드 포인터. 없으면 NULL 반환
 */
LogNode *find_log_node_by_pid(char *pid_str) {
    char tmp_log[5000];
    LogNode *tmp = log_list_head;
    
    while (tmp != NULL) {
        strcpy(tmp_log, tmp->log_data);
        char *extracted_pid = strtok(tmp_log, " ");
        if (extracted_pid != NULL && strcmp(extracted_pid, pid_str) == 0) {
            return tmp;
        }
        tmp = tmp->next;
    }
    return NULL;
}

/*
 * 프로그램 시작 시 사용자 홈 경로와 백업 디렉토리 경로를 설정하고, 로그 파일을 로드하여 초기화한다.
 */
void init_environment(void) {
    strcpy(home_dir, getenv("HOME"));
    snprintf(backup_dir, sizeof(backup_dir), "%s/backup", home_dir);
    
    if (access(backup_dir, F_OK) != 0) {
        mkdir(backup_dir, 0777);
    }
    
    char log_path[MAX_PATHLENGTH + 64];
    snprintf(log_path, sizeof(log_path), "%s/monitor_list.log", backup_dir);

    if ((monitor_list_fd = open(log_path, O_RDWR | O_CREAT | O_APPEND | O_SYNC, 0666)) < 0) {
        fprintf(stderr, "open error for %s\n", log_path);
        exit(1);
    }
    load_monitor_list();
}

/*
 * 문자열로 입력된 내장 명령어를 식별하여 고유의 정수 코드로 변환한다.
 * char *command: 입력된 명령어 문자열
 * return : int, 명령어에 매칭되는 정수. 매칭 실패 시 0 반환
 */
int parse_command(char *command) {
    if (!command) return 0;
    if (strcmp(command, "add") == 0) return 1;
    if (strcmp(command, "remove") == 0) return 2;
    if (strcmp(command, "list") == 0) return 3;
    if (strcmp(command, "help") == 0) return 4;
    if (strcmp(command, "exit") == 0) return 5;
    return 0;
}

/*
 * 상대 경로 또는 절대 경로를 파싱하여 유효한 절대 경로(absolute path)로 변환한다.
 * char *path: 파싱할 대상 경로
 * return : char*, 파싱된 절대 경로 문자열. 실패 또는 에러 시 NULL 반환
 */
char *get_absolute_path(char *path) {
    char tmp[MAX_PATHLENGTH];
    if ((realpath(path, tmp)) == NULL) {
        int err = errno;
        if (err == EINVAL) {
            fprintf(stderr, "Invalid Path\n");
            return NULL;
        } else if (err == ENAMETOOLONG) {
            fprintf(stderr, "Name too long\n");
            return NULL;
        } else if (err == ENOENT) { 
            fprintf(stderr, "%s : No such file or directory\n", path);
            return NULL;
        } else {
            fprintf(stderr, "realpath error\n");
            return NULL;
        }
    }
    return strdup(tmp);
}

/*
 * 입력받은 경로가 사용자 홈 디렉토리 내부인지, 백업 디렉토리를 포함하지 않는지 검증한다.
 * char *path: 검증할 절대 경로
 * return : bool, 유효한 경로면 true, 접근 불가능한 경로면 false 반환
 */
bool is_valid_path(char *path) {
    if (strcmp(path, home_dir) == 0) {
        fprintf(stderr, "Home Directory itself is not allowed\n");
        return false;
    }
    if (strncmp(path, backup_dir, strlen(backup_dir)) == 0) {
        fprintf(stderr, "Path Under Backup Directory is not allowed\n");
        return false;
    }
    if (strncmp(path, home_dir, strlen(home_dir)) != 0) {
        fprintf(stderr, "Path not under home directory is not allowed\n");
        return false;
    }
    return true;
}

/*
 * 데몬 프로세스 생성 시 PID와 대상 경로를 monitor_list.log 파일 끝에 기록한다.
 * pid_t pid: 데몬 프로세스의 PID
 * char *path: 모니터링 대상 절대 경로
 */
void append_to_monitor_list_file(pid_t pid, char *path) {
    char buffer[MAX_PATHLENGTH + 64];
    int length = snprintf(buffer, sizeof(buffer), "%d : %s\n", pid, path);
    if (write(monitor_list_fd, buffer, length) < 0)
        fprintf(stderr, "write error\n");
}
