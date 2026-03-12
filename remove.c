#include "sync.h"

/*
 * remove 명령어를 처리하여 특정 PID를 가진 데몬 프로세스를 종료하고 모니터링을 중단한다.
 * int argc: 입력된 토큰의 개수
 * char *argv[]: 토큰 문자열 배열 (ex: ["remove", "5539"])
 */
void cmd_remove(int argc, char *argv[]) {
    // 예외 처리 : 잘못된 인자 개수
    if (argc != 2) {
        fprintf(stderr, "ERROR: <DAEMON_PID> is required\n");
        print_usage(2, 1); 
        return;
    }

    char *pid_str = argv[1];
    pid_t target_pid = (pid_t)atoi(pid_str);

    // 예외 처리: pid 유효성 검증 
    if (target_pid <= 0) {
        fprintf(stderr, "ERROR: Invalid PID format ('%s')\n", pid_str);
        return;
    }

    // 모니터링 목록에서 해당 PID의 노드 검색
    LogNode *target_node = find_log_node_by_pid(pid_str);
    
    // 예외 처리: "monitor_list.log"에 없는 경우
    if (target_node == NULL) {
        fprintf(stderr, "ERROR: Daemon process with PID %s is not in monitor_list.log\n", pid_str);
        return;
    }

    // 로그 포맷: "PID : PATH"
    char log_copy[MAX_PATHLENGTH * 2];
    strcpy(log_copy, target_node->log_data);

    strtok(log_copy, " ");     // PID 추출 후 무시
    strtok(NULL, " ");         // ":" 추출 후 무시
    char *monitored_path = strtok(NULL, " "); // 대상 PATH 추출

    // 모니터링 종료 성공 메시지 출력
    if (monitored_path != NULL) {
        printf("monitoring ended (%s) : %s\n", monitored_path, pid_str);
    }

    // 모니터링 목록에서 해당 데몬 프로세스 정보 삭제 및 파일 덮어쓰기 
    remove_log_node(target_node);
    sync_monitor_list_file();

    // 데몬 프로세스에 SIGUSR1 시그널을 보내 종료를 지시 
    // 데몬 내부의 signal_handler가 이를 받아 로그 파일 및 백업 디렉토리를 스스로 삭제함
    if (kill(target_pid, SIGUSR1) < 0) {
        // 이미 종료되었거나 권한이 없는 경우에 대한 에러 출력
        fprintf(stderr, "Failed to send signal to daemon");
    }
}
