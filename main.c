#include "sync.h"

/* 전역 변수 실체 정의 */
char home_dir[MAX_PATHLENGTH];
char backup_dir[MAX_PATHLENGTH + 16];
int monitor_list_fd;
LogNode *log_list_head = NULL;

/*
 * 사용자로부터 명령어를 입력받아 파싱하고, 각 명령어에 맞는 핸들러 함수를 호출한다.
 */
int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    // 프로그램 환경 초기화 (경로 설정 및 기존 로그 로드)
    init_environment();

    char input_buffer[MAX_PATHLENGTH];
    char *tokens[64]; // 명령어 인자들을 저장할 배열
    int token_count;

    while (1) {
        // 프롬프트 출력
        printf("> ");
        fflush(stdout);

        // 한 줄 입력 받기
        if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL) {
            break;
        }

        // 개행 문자 제거
        input_buffer[strcspn(input_buffer, "\n")] = '\0';

        // 공백만 입력하거나 엔터만 친 경우 예외 처리 
        if (strlen(input_buffer) == 0) {
            continue;
        }

        // 문자열 토큰화 (명령어 및 인자 분리)
        token_count = 0;
        char *ptr = strtok(input_buffer, " ");
        while (ptr != NULL && token_count < 63) {
            tokens[token_count++] = ptr;
            ptr = strtok(NULL, " ");
        }
        tokens[token_count] = NULL;

        if (token_count == 0) continue;

        // 첫 번째 토큰(명령어) 식별
        int command_code = parse_command(tokens[0]);

        // 명령어 코드에 따른 함수 분기
        switch (command_code) {
            case 1: // add
                cmd_add(token_count, tokens);
                break;
            case 2: // remove
                cmd_remove(token_count, tokens);
                break;
            case 3: // list
                cmd_list(token_count, tokens);
                break;
            case 4: // help
                print_usage_all();
                break;
            case 5: // exit
                printf("exit program\n"); // 프로그램 종료 메시지 출력
                close(monitor_list_fd);
                exit(0);
            default:
                // 정의되지 않은 명령어 입력 시 도움말 출력
                print_usage_all();
                break;
        }
    }

    return 0;
}
