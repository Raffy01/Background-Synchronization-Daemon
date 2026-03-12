#define OPENSSL_API_COMPAT 0x10100000L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <openssl/md5.h>
#include <sys/wait.h>
#include <dirent.h>

#define PATH_LENGTH 4096

// 모니터링 대상 파일의 정보를 담는 연결 리스트 노드 구조체
typedef struct target_node {
    char path[PATH_LENGTH];
    time_t mtime;
    bool is_removed;
    char hash[33];
    struct target_node *next;
} TargetNode;

// 전역 변수
pid_t daemon_pid;
char backup_dir[PATH_LENGTH];
int log_fd;
char pid_str[16];
char base_path[PATH_LENGTH];
TargetNode *target_list_head = NULL;

/*
 * 데몬 종료 시그널(SIGUSR1)을 받았을 때 호출되는 핸들러.
 * PID.log 파일을 삭제하고 백업 폴더(PID/)를 재귀적으로 삭제한 뒤 종료한다.
 * int signo: 발생한 시그널 번호
 */
void signal_handler(int signo) {
    (void)signo;
    char target_log[PATH_LENGTH * 2];
    char target_backup_dir[PATH_LENGTH * 2];

    snprintf(target_log, sizeof(target_log), "%s/%s.log", backup_dir, pid_str);
    snprintf(target_backup_dir, sizeof(target_backup_dir), "%s/%s", backup_dir, pid_str);

    remove(target_log); // PID.log 파일 삭제

    pid_t child_pid = fork();
    if (child_pid == 0) {
        // 백업 디렉토리 삭제
        execlp("rm", "rm", "-rf", target_backup_dir, (char *)NULL);
        exit(1);
    } else if (child_pid > 0) {
        waitpid(child_pid, NULL, 0); // 좀비 프로세스 방지
    }
    
    exit(0);
}

/*
 * 대상 파일의 MD5 해시값을 계산하여 32자리의 16진수 문자열로 반환한다.
 * 바이너리 비교 버그를 완벽하게 차단하는 핵심 로직.
 * const char *path: 해시를 계산할 대상 파일 경로
 * char *output_hash: 계산된 해시 문자열을 저장할 버퍼
 */
void calculate_md5_hex(const char *path, char *output_hash) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        memset(output_hash, 0, 33);
        return;
    }

    MD5_CTX ctx;
    unsigned char buf[4096];
    unsigned char raw_hash[MD5_DIGEST_LENGTH];
    int bytes;

    MD5_Init(&ctx);
    while ((bytes = read(fd, buf, sizeof(buf))) > 0) {
        MD5_Update(&ctx, buf, bytes);
    }
    MD5_Final(raw_hash, &ctx);
    close(fd);

    // 바이너리 해시를 문자열(Hex)로 변환
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(output_hash + (i * 2), "%02x", raw_hash[i]);
    }
    output_hash[32] = '\0';
}

/*
 * 파일 변경 이력을 PID.log 파일에 기록한다.
 * const char *command: 수행 내용 (create, modify, remove)
 * const char *path: 대상 파일의 절대 경로
 */
void write_daemon_log(const char *command, const char *path) {
    char buf[PATH_LENGTH + 128];
    time_t current_time = time(NULL);
    struct tm *now = localtime(&current_time);

    // [수행 시간] [수행 내용] [절대경로] 포맷
    int len = snprintf(buf, sizeof(buf), "[%04d-%02d-%02d %02d:%02d:%02d][%s][%s]\n",
             now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,
             now->tm_hour, now->tm_min, now->tm_sec,
             command, path);

    lseek(log_fd, 0, SEEK_END);
    if (write(log_fd, buf, len) < 0) {
        // 백그라운드 데몬이므로 에러 출력 대신 무시
    }
}

/*
 * 변경이 감지된 원본 파일을 백업 디렉토리에 (파일명_YYYYMMDDHHMMSS) 형태로 복사한다.
 * 하위 디렉토리 구조를 원본과 동일하게 유지하여 생성한다.
 * const char *path: 복사할 원본 파일의 절대 경로
 * const char *command: "create" 또는 "modify"
 */
void make_backup_file(const char *path, const char *command) {
    char backup_target_dir[PATH_LENGTH * 2];
    snprintf(backup_target_dir, sizeof(backup_target_dir), "%s/%s", backup_dir, pid_str);

    // 최상위 PID 백업 디렉토리가 없으면 생성
    if (access(backup_target_dir, F_OK) != 0) {
        mkdir(backup_target_dir, 0777);
    }

    // 파일 복사 전, 로그 작성
    write_daemon_log(command, path);

    // 시간 포맷(YYYYMMDDHHMMSS) 생성
    time_t current_time = time(NULL);
    struct tm *now = localtime(&current_time);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y%m%d%H%M%S", now);

    // 파일명 추출
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;

    // 디렉토리 구조 유지 (base_path를 기준으로 상대 경로 추출)
    if (strncmp(path, base_path, strlen(base_path)) == 0) {
        const char *rel_path = path + strlen(base_path);
        if (rel_path[0] == '/') rel_path++; // 시작 슬래시 제거

        char tmp_rel[PATH_LENGTH];
        strcpy(tmp_rel, rel_path);

        // 마지막 '/' 색인
        char *last_slash = strrchr(tmp_rel, '/');
        if (last_slash != NULL) {
            *last_slash = '\0'; // 마지막 '/'를 널 문자로 덮어써서 디렉토리 경로만 남김
            // 디렉토리 생성
            char *token = strtok(tmp_rel, "/");
            while (token != NULL) {
                strcat(backup_target_dir, "/");
                strcat(backup_target_dir, token);
                if (access(backup_target_dir, F_OK) != 0) {
                    mkdir(backup_target_dir, 0777);
                }
                token = strtok(NULL, "/");
            }
        }
    }

    // 최종 백업 파일 경로 조립: 백업폴더/디렉토리경로/파일명_시간
    char final_backup_path[PATH_LENGTH * 2];
    snprintf(final_backup_path, sizeof(final_backup_path), "%s/%s_%s", backup_target_dir, filename, time_str);

    // 파일 복사
    int fd_src = open(path, O_RDONLY);
    if (fd_src < 0) return;
    
    int fd_dest = open(final_backup_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd_dest < 0) {
        close(fd_src);
        return;
    }

    char buf[4096];
    int count;
    while ((count = read(fd_src, buf, sizeof(buf))) > 0) {
        if (write(fd_dest, buf, count) < 0) break;
    }

    close(fd_src);
    close(fd_dest);
}

/*
 * 모니터링 대상 파일을 연결 리스트에 추가하고 초기 백업본을 생성한다.
 * const char *path: 대상 파일 경로
 */
void add_target_node(const char *path) {
    TargetNode *tmp = (TargetNode *)malloc(sizeof(TargetNode));
    struct stat statbuf;
    
    if (stat(path, &statbuf) < 0) {
        free(tmp);
        return;
    }

    strcpy(tmp->path, path);
    tmp->mtime = statbuf.st_mtime;
    calculate_md5_hex(path, tmp->hash);
    tmp->is_removed = false;
    tmp->next = NULL;

    make_backup_file(path, "create"); // 최초 감지 시 create 백업

    if (target_list_head == NULL) {
        target_list_head = tmp;
    } else {
        TargetNode *curr = target_list_head;
        while (curr->next != NULL) curr = curr->next;
        curr->next = tmp;
    }
}

/*
 * scandir 함수에서 현재 디렉토리(.)와 부모 디렉토리(..)를 제외하기 위한 필터 함수
 */
static int filter_dots(const struct dirent *dir) {
    if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) return 0;
    return 1;
}

/*
 * 입력받은 디렉토리를 탐색하며 모니터링 대상 파일을 수집한다.
 * BFS(파일 우선 탐색) 요구조건을 충족하면서 배열 동적 할당 없이 메모리를 즉시 해제한다.
 * const char *path: 탐색 대상 경로
 * int is_recursive: 1이면 하위 폴더까지 재귀 탐색 (-r 옵션)
 */
void scan_and_add_targets(const char *path, int is_recursive) {
    struct dirent **namelist;
    
    // filter_dots를 사용하여 . 과 .. 을 애초에 배제함
    int count = scandir(path, &namelist, filter_dots, alphasort);
    if (count < 0) return;

    char tmp[PATH_LENGTH + 256];

    // 파일을 먼저 리스트에 추가하고 즉시 메모리 해제
    for (int i = 0; i < count; i++) {
        snprintf(tmp, sizeof(tmp), "%s/%s", path, namelist[i]->d_name);
        
        struct stat statbuf;
        // stat 검사 성공 및 디렉토리인 경우에만 포인터를 살려둠
        if (stat(tmp, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
            // 디렉토리는 2차 탐색(재귀)을 위해 남겨둔다.
            continue; 
        } 
        
        // 파일인 경우 감시 목록에 추가
        if (stat(tmp, &statbuf) == 0 && !S_ISDIR(statbuf.st_mode)) {
            add_target_node(tmp); 
        }

        // 처리가 끝난 파일(또는 stat 실패 항목)의 메모리를 즉시 해제하고 널 처리
        free(namelist[i]);
        namelist[i] = NULL;
    }

    // 디렉토리 재귀 탐색 및 최종 메모리 해제
    for (int i = 0; i < count; i++) {
        if (namelist[i] != NULL) {
            // -r 옵션이 켜져 있을 때만 하위 디렉토리로 파고듦
            if (is_recursive) {
                snprintf(tmp, sizeof(tmp), "%s/%s", path, namelist[i]->d_name);
                scan_and_add_targets(tmp, 1);
            }
            
            // 재귀 처리가 끝났거나 -r 옵션이 없어서 버려지는 디렉토리들의 메모리 해제
            free(namelist[i]);
        }
    }

    free(namelist);
}

/*
 * 설정된 주기(period)마다 파일들의 상태를 검사하여 변경사항을 백업하고 로그를 남긴다.
 * int period: 감시 주기 (초)
 */
void start_monitoring(int period) {
    while (1) {
        sleep(period); // 주기만큼 대기
        
        TargetNode *curr = target_list_head;
        while (curr != NULL) {
            struct stat statbuf;
            
            // 파일이 삭제된 경우
            if (stat(curr->path, &statbuf) < 0) {
                if (!curr->is_removed) {
                    curr->is_removed = true;
                    write_daemon_log("remove", curr->path); // 삭제는 로그만 남기고 백업 안 함
                }
            } 
            // 파일이 존재하거나 다시 생성된 경우
            else {
                // 삭제되었다가 다시 생성된 경우
                if (curr->is_removed) {
                    curr->is_removed = false;
                    curr->mtime = statbuf.st_mtime;
                    calculate_md5_hex(curr->path, curr->hash);
                    make_backup_file(curr->path, "create");
                } 
                // 수정된 경우 (시간이 바뀌었거나 해시값이 바뀌었을 때)
                else {
                    if (statbuf.st_mtime != curr->mtime) {
                        curr->mtime = statbuf.st_mtime;
                        
                        char new_hash[33];
                        calculate_md5_hex(curr->path, new_hash);
                        
                        // 문자열(Hex)로 안전하게 비교
                        if (strcmp(curr->hash, new_hash) != 0) {
                            strcpy(curr->hash, new_hash);
                            make_backup_file(curr->path, "modify");
                        }
                    }
                }
            }
            curr = curr->next;
        }
    }
}

/*
 * 데몬 프로세스의 메인 진입점.
 * 파라미터: 대상경로, 재귀옵션(1/0), 주기(초), 백업폴더경로
 */
int main(int argc, char *argv[]) {
    if (argc < 5) exit(1);

    // 인자 파싱 및 전역 변수 초기화
    strcpy(base_path, argv[1]);
    int is_recursive = atoi(argv[2]);
    int period = atoi(argv[3]);
    strcpy(backup_dir, argv[4]);

    daemon_pid = getpid();
    snprintf(pid_str, sizeof(pid_str), "%d", daemon_pid);

    // PID.log 경로 설정
    char log_path[PATH_LENGTH * 2];
    snprintf(log_path, sizeof(log_path), "%s/%s.log", backup_dir, pid_str);

    // 시그널 핸들러 등록 (remove 명령어가 날리는 SIGUSR1 대기)
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    // 프로세스를 완벽한 데몬(Daemon)으로 승격
    setsid(); // 새로운 세션 생성 및 제어 터미널 분리
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    // 열려있는 모든 표준 파일 디스크립터 닫기 및 /dev/null 연결
    int maxfd = getdtablesize();
    for (int fd = 0; fd < maxfd; fd++) {
        close(fd);
    }
    umask(0);
    if (chdir("/") < 0) exit(1); // 작업 디렉토리를 루트로 변경

    int fd_null = open("/dev/null", O_RDWR);
    if (dup(fd_null) < 0) exit(1); // stdout 연결
    if (dup(fd_null) < 0) exit(1); // stderr 연결

    // 백그라운드에서 독자적으로 사용할 PID.log 파일 열기
    if ((log_fd = open(log_path, O_RDWR | O_CREAT | O_APPEND, 0666)) < 0) {
        exit(1);
    }

    struct stat statbuf;
    if (stat(base_path, &statbuf) < 0) exit(1);

    // 파일/디렉토리 여부에 따라 초기 모니터링 목록 구축
    if (!S_ISDIR(statbuf.st_mode)) {
        add_target_node(base_path);
    } else {
        scan_and_add_targets(base_path, is_recursive);
    }

    start_monitoring(period);

    return 0; // 도달하지 않음
}
