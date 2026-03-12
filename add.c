#include "sync.h"

/*
 * add 명령어를 처리하여 새로운 데몬 프로세스를 생성하고 모니터링을 시작한다.
 * int argc: 입력된 토큰의 개수
 * char *argv[]: 토큰 문자열 배열 (예: ["add", "target_dir", "-r", "-t", "3"])
 */
void cmd_add(int argc, char *argv[]) {
    // 예외 처리 : 인자 개수가 적음
    if (argc < 2) {
        fprintf(stderr, "ERROR: Path not included\n");
        print_usage(1, 1); 
        return;
    }

    // 절대 경로 변환
    char *target_path = get_absolute_path(argv[1]);
    if (target_path == NULL) return;
    // 예외 처리 : 잘못된 경로
    if (!is_valid_path(target_path)) {
        free(target_path);
        return;
    }

    // 옵션 파싱 (-d, -r, -t <PERIOD>)
    bool d_flag = false;
    bool r_flag = false;
    int period = 1; // 디폴트 주기 1초

    // 경로는 argv[1]에 있으므로, 옵션은 argv[2]부터 검사
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            d_flag = true;
        } else if (strcmp(argv[i], "-r") == 0) {
            r_flag = true;
        } else if (strcmp(argv[i], "-t") == 0) {
            // -t 옵션 뒤에 숫자가 따라오는지 확인
            if (i + 1 < argc) {
                period = atoi(argv[++i]);
                if (period <= 0) {
                    // 예외 처리 : 잘못된 주기 값
                    fprintf(stderr, "ERROR: Invalid period value\n");
                    free(target_path);
                    return;
                }
            } else {
                // 예외 처리 : 주기 값 없음
                fprintf(stderr, "ERROR: -t option requires a period\n");
                free(target_path);
                return;
            }
        } else {
            // 예외 처리 : 잘못된 옵션
            fprintf(stderr, "ERROR: Unknown option '%s'\n", argv[i]);
            print_usage(1, 1);
            free(target_path);
            return;
        }
    }

    // 대상의 파일/디렉토리 여부에 따른 옵션 매칭 검증
    struct stat statbuf;

    // 예외 처리 : stat 에러
    if (lstat(target_path, &statbuf) < 0) {
        perror("ERROR: stat error");
        free(target_path);
        return;
    }

    if (S_ISDIR(statbuf.st_mode)) { // 디렉토리인 경우
        if (!d_flag && !r_flag) {
            fprintf(stderr, "ERROR: '%s' is a directory but -d or -r option is not given\n", target_path);
            free(target_path);
            return;
        }
    } else { // 파일인 경우
        if (d_flag || r_flag) {
            fprintf(stderr, "ERROR: '%s' is a file but -d or -r option is given\n", target_path);
            free(target_path);
            return;
        }
    }

    // 중복 모니터링 검사
    LogNode *tmp = log_list_head;
    while (tmp != NULL) {
        char log_copy[MAX_PATHLENGTH + 1];
        strcpy(log_copy, tmp->log_data);
        
        // 로그 포맷: "PID : PATH"
        strtok(log_copy, " "); // PID 건너뛰기
        strtok(NULL, " ");     // ":" 건너뛰기
        char *monitored_path = strtok(NULL, " "); // 경로 추출

        if (monitored_path && strcmp(monitored_path, target_path) == 0) {
            fprintf(stderr, "ERROR: '%s' is already being monitored\n", target_path);
            free(target_path);
            return;
        }
        tmp = tmp->next;
    }

    // 데몬 프로세스 생성
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "fork error");
        free(target_path);
        return;
    } else if (pid == 0) {  //자식 프로세스
        char r_flag_str[8];
        char period_str[16];
        
        // 데몬에게 넘겨줄 인자를 문자열로 변환
        snprintf(r_flag_str, sizeof(r_flag_str), "%d", r_flag ? 1 : 0);
        snprintf(period_str, sizeof(period_str), "%d", period);

        // 데몬 프로세스 실행
        execl("./daemon", "./daemon", target_path, r_flag_str, period_str, backup_dir, (char *)NULL);
        
        // execl이 실패했을 경우에만 아래 코드가 실행됨
        fprintf(stderr, "execl error");
        exit(1); 
    } else {    //부모 프로세스
        append_to_monitor_list_file(pid, target_path); // 로그 파일에 기록
        add_daemon_to_list(pid, target_path);          // 메모리 연결 리스트에 추가
        
        printf("monitoring started (%s) : %d\n", target_path, pid);
    }

    free(target_path); // 최종 메모리 해제
}
