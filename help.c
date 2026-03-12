#include "sync.h"

/* * 특정 명령어의 사용법(Usage)을 화면에 출력한다.
 * int command: 출력할 명령어의 고유 코드 (1: add, 2: remove, 3: list, 4: help, 5: exit)
 * int is_error: 1이면 단일 에러 메시지("Usage: ")로 출력, 0이면 전체 리스트 출력용
 */
void print_usage(int command, int is_error) {
    if (is_error) {
        printf("Usage:\n");
    }

    // 들여쓰기와 프롬프트 기호 출력
    const char *prefix = is_error ? "  >" : "  >";

    switch (command) {
        case 1: // add
            printf("%s add <PATH> [OPTION]... : add new daemon process of <PATH> if <PATH> is file\n", prefix);
            printf("    -d : add new daemon process of <PATH> if <PATH> is directory\n");
            printf("    -r : add new daemon process of <PATH> recursive if <PATH> is directory\n");
            printf("    -t <PERIOD> : set daemon process time to <PERIOD> sec (default: 1sec)\n");
            break;
        case 2: // remove
            printf("%s remove <DAEMON_PID> : delete daemon process with <DAEMON_PID>\n", prefix);
            break;
        case 3: // list
            printf("%s list [DAEMON_PID] : show daemon process list or dir tree\n", prefix);
            break;
        case 4: // help
            printf("%s help [COMMAND] : show commands for program\n", prefix);
            break;
        case 5: // exit
            printf("%s exit : exit program\n", prefix);
            break;
        default:
            break;
    }
}

/*
 * 프로그램의 모든 내장 명령어에 대한 전체 사용법(Usage)을 출력한다.
 */
void print_usage_all(void) {
    printf("Usage:\n");
    for (int i = 1; i <= 5; i++) {
        print_usage(i, 0); // 0을 넘겨 "Usage:"가 중복 출력되지 않게 함
    }
}
