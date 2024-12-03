#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_LINE 256 // 최대 입력 줄 길이
#define MAX_ARGS 50  // 명령어의 최대 인자 수

// SIGINT(Ctrl-C) 시그널 처리
void handle_sigint(int sig) {
    printf("\nCaught signal %d (SIGINT). Exiting gracefully...\n", sig);
    exit(0); // 프로그램 종료
}

// SIGQUIT(Ctrl-\) 시그널 처리
void handle_sigquit(int sig) {
    printf("\nCaught signal %d (SIGQUIT). Ignoring and resuming...\n", sig);
    // 기본 동작 없이 계속 실행
}

// SIGTSTP(Ctrl-Z) 시그널 처리
void handle_sigtstp(int sig) {
    printf("\nCaught signal %d (SIGTSTP). Stopping is disabled. Resuming...\n", sig);
    // 기본 동작 없이 계속 실행
}

// 명령어를 공백 단위로 분리하여 인자 배열로 변환하는 함수
int getargs(char *cmd, char **argv);

int main() {
    char buf[MAX_LINE];     // 사용자 입력 저장 버퍼
    char *argv[MAX_ARGS];   // 명령어와 인자 배열
    pid_t pid;              // 프로세스 ID를 저장할 변수

    // 시그널 핸들러 등록
    signal(SIGINT, handle_sigint);  // Ctrl-C
    signal(SIGQUIT, handle_sigquit); 
    signal(SIGTSTP, handle_sigtstp); // Ctrl-Z

//  printf("Mini Shell running. Use Ctrl-C to exit, 
        // Ctrl-\\ to test SIGQUIT, or Ctrl-Z to test SIGTSTP handling.\n");

    while (1) { // 무한 루프를 통해 쉘 유지
        printf("shell> ");  // 쉘 프롬프트 출력
        fflush(stdout);     // 출력 버퍼 비우기 (즉시 출력)

        if (!fgets(buf, sizeof(buf), stdin)) break; // EOF 처리
        buf[strcspn(buf, "\n")] = '\0'; // 줄바꿈 문자 제거

        if (strlen(buf) == 0) continue; // 빈 입력 무시

        int narg = getargs(buf, argv); // 명령어와 인자 분리

        if (strcmp(argv[0], "exit") == 0) break; // "exit" 명령어로 종료

        pid = fork(); // 새로운 프로세스 생성

        if (pid == 0) { // 자식 프로세스
            execvp(argv[0], argv); // 명령어 실행
            perror("execvp failed"); // 실행 실패 시 오류 출력
            exit(EXIT_FAILURE);      // 자식 프로세스 종료
        } else if (pid > 0) { // 부모 프로세스
            wait(NULL); // 자식 프로세스 종료 대기
        } else { // fork 실패 시
            perror("fork failed");
        }
    }
    return 0; // 정상 종료
}

// 사용자 입력을 공백 단위로 분리하여 명령어와 인자 배열 생성
int getargs(char *cmd, char **argv) {
    int narg = 0; // 인자 개수 저장

    while (*cmd) {
        // 공백 또는 탭 문자를 NULL로 치환하여 단어를 분리
        while (*cmd == ' ' || *cmd == '\t') *cmd++ = '\0';

        if (*cmd) argv[narg++] = cmd; // 단어 시작 위치 저장

        // 다음 공백 또는 NULL 문자까지 이동
        while (*cmd && *cmd != ' ' && *cmd != '\t') cmd++;
    }

    argv[narg] = NULL; // 마지막 인자는 NULL로 설정 (execvp 요구사항)
    return narg; // 인자 개수 반환
}