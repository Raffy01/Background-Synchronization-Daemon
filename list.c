#include "sync.h"

// 로그의 명령어와 시간을 저장할 구조체
typedef struct {
    char cmd[32];
    char time[64];
} LogEntry;

/*
 * scandir 함수에서 현재 디렉토리(.)와 부모 디렉토리(..)를 제외하기 위한 필터 함수
 * const struct dirent *dir: 검사할 디렉토리 엔트리 구조체 포인터
 * 반환값(return): 포함할 경우 1, 제외할 경우 0 반환
 */
static int filter_dots(const struct dirent *dir) {
    if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) return 0;
    return 1; // 그 외 파일 및 디렉토리만 포함
}

/*
 * 뎁스(Depth)와 마지막 항목 여부 배열을 기반으로 트리 선(┣, ┃, ┗)을 출력한다.
 * int depth: 선을 그릴 트리의 현재 깊이
 * int *is_last: 각 뎁스별로 마지막 항목인지 여부를 저장하는 배열 (1이면 마지막, 0이면 진행 중)
 */
static void print_tree_prefix(int depth, int *is_last) {
    for (int i = 0; i < depth; i++) {
        if (i == depth - 1) {
            // 본인 차례: 마지막 항목이면 ┗, 아니면 ┣
            if (is_last[i]) printf("┗ ");
            else printf("┣ ");
        } else {
            // 부모의 선을 이어받는 공간: 부모가 마지막 항목이었으면 공백, 아니면 ┃ (수직선)
            if (is_last[i]) printf("  ");
            else printf("┃ ");
        }
    }
}

/*
 * PID.log 파일에서 특정 파일(target_path)에 대한 변경 기록을 모아 트리 형태로 출력한다.
 * const char *target_path: 로그를 검색할 대상 파일의 절대 경로
 * int log_fd: 읽어들일 PID.log 파일의 파일 디스크립터
 * int depth: 트리 구조 출력을 위한 현재의 뎁스
 * int *is_last: 각 뎁스별 마지막 항목 여부 배열 (로그 항목들의 선을 그릴 때 사용)
 */
static void print_file_logs(const char *target_path, int log_fd, int depth, int *is_last) {
    char buf[MAX_PATHLENGTH * 2];
    char c;
    int i = 0;

    // 동적 배열로 매칭되는 로그들을 모두 수집
    int capacity = 10;
    LogEntry *entries = malloc(capacity * sizeof(LogEntry));
    int entry_count = 0;

    lseek(log_fd, 0, SEEK_SET);

    while (read(log_fd, &c, 1) > 0) {
        buf[i++] = c;
        if (c == '\n' || c == EOF) {
            buf[i - 1] = '\0';
            
            char log_time[64] = {0};
            char log_cmd[32] = {0};
            char log_path[MAX_PATHLENGTH] = {0};

            if (sscanf(buf, "[%[^]]][%[^]]][%[^]]]", log_time, log_cmd, log_path) == 3) {
                // 경로가 일치하는 로그만 저장
                if (strcmp(target_path, log_path) == 0) {
                    if (entry_count >= capacity) {
                        capacity *= 2;
                        entries = realloc(entries, capacity * sizeof(LogEntry));
                    }
                    strcpy(entries[entry_count].cmd, log_cmd);
                    strcpy(entries[entry_count].time, log_time);
                    entry_count++;
                }
            }
            i = 0;
            memset(buf, 0, sizeof(buf));
        }
    }

    // 수집한 로그들을 트리 기호와 함께 출력
    for (int k = 0; k < entry_count; k++) {
        // 현재 로그가 해당 파일의 마지막 로그인지 체크
        is_last[depth] = (k == entry_count - 1);
        print_tree_prefix(depth + 1, is_last);
        printf("[%s] [%s]\n", entries[k].cmd, entries[k].time);
    }

    free(entries);
}

/*
 * 디렉토리 트리를 재귀적으로 탐색하며 파일 구조와 로그를 출력한다.
 * const char *current_path: 탐색을 시작할(또는 현재 탐색 중인) 디렉토리/파일의 절대 경로
 * int depth: 트리 구조 출력을 위한 들여쓰기 깊이 (하위 폴더로 갈수록 1씩 증가)
 * int log_fd: 변경 기록이 저장된 PID.log 파일의 파일 디스크립터
 * int *is_last: 뎁스별로 마지막 항목인지 여부를 저장하는 배열 (최대 깊이 1024 가정)
 */
static void print_tree(const char *current_path, int depth, int log_fd, int *is_last) {
    struct dirent **namelist;
    // ".", ".." 제외
    int count = scandir(current_path, &namelist, filter_dots, alphasort);

    if (count < 0) { // 파일인 경우
        if (depth == 1) {
            const char *filename = strrchr(current_path, '/');
            filename = filename ? filename + 1 : current_path;
            
            is_last[0] = 1; // 최상위 파일이므로 is_last에 마킹
            print_tree_prefix(1, is_last);
            printf("%s\n", filename);
            print_file_logs(current_path, log_fd, 1, is_last);
        }
        return;
    }

    for (int i = 0; i < count; i++) {
        // 현재 탐색 중인 항목이 이 디렉토리의 마지막 항목인지 체크
        is_last[depth - 1] = (i == count - 1); 

        print_tree_prefix(depth, is_last);
        
        char next_path[MAX_PATHLENGTH * 2 + 2];
        snprintf(next_path, sizeof(next_path), "%s/%s", current_path, namelist[i]->d_name);

        struct stat statbuf;
        if (lstat(next_path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
            printf("%s/\n", namelist[i]->d_name);
            print_tree(next_path, depth + 1, log_fd, is_last); // 재귀 탐색
        } else {
            printf("%s\n", namelist[i]->d_name);
            print_file_logs(next_path, log_fd, depth, is_last); // 파일 로그 출력
        }
        free(namelist[i]);
    }
    free(namelist);
}

/*
 * list 명령어를 처리하여 모니터링 중인 데몬 목록이나 특정 데몬의 관리 트리를 출력한다.
 * int argc: 입력된 토큰의 개수
 * char *argv[]: 토큰 문자열 배열 (예: ["list"] 또는 ["list", "5539"])
 */
void cmd_list(int argc, char *argv[]) {
    if (argc == 1) { // [Daemon PID]가 입력되지 않은 경우 모든 로그 출력
        LogNode *tmp = log_list_head;
        // 예외 처리 : 로그가 빈 경우
        if (tmp == NULL) {
            printf("Nothing is under monitoring\n");
            return;
        }
        while (tmp != NULL) {
            printf("%s\n", tmp->log_data);
            tmp = tmp->next;
        }
    } else if (argc == 2) { //[Daemon PID]가 입력된 경우
        char *pid_str = argv[1];
        LogNode *target_node = find_log_node_by_pid(pid_str);
        // 예외 처리 :  잘못된 PID 값
        if (target_node == NULL) {
            fprintf(stderr, "ERROR: Invalid PID : %s. No such daemon process\n", pid_str);
            return;
        }

        char log_copy[MAX_PATHLENGTH * 2];
        strcpy(log_copy, target_node->log_data);
        strtok(log_copy, " "); 
        strtok(NULL, " ");     
        char *base_path = strtok(NULL, " "); 

        if (base_path == NULL) return;

        char pid_log_path[MAX_PATHLENGTH * 2 + 4];
        snprintf(pid_log_path, sizeof(pid_log_path), "%s/%s.log", backup_dir, pid_str);
        
        int pid_fd = open(pid_log_path, O_RDONLY);
        if (pid_fd < 0) {
            fprintf(stderr, "ERROR: Cannot open log file for PID %s\n", pid_str);
            return;
        }

        // 트리 출력 시 뎁스별 마지막 항목 여부를 저장할 배열
        int is_last[1024] = {0}; 

        printf("%s\n", base_path);
        print_tree(base_path, 1, pid_fd, is_last);
        
        close(pid_fd);
    } else {
        fprintf(stderr, "ERROR: Invalid number of arguments. type 'list [PID]'\n");
        print_usage(3, 1);
    }
}
