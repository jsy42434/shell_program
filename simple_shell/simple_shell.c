#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_LINE 256 // 입력 가능한 최대 줄 길이
#define MAX_ARGS 50  // 명령어의 최대 인자 수

// 사용자 입력을 공백 단위로 분리하여 명령어와 인자를 추출
int getargs(char *cmd, char **argv);

int main() {
    char buf[MAX_LINE]; // 사용자 입력을 저장할 버퍼
    char *argv[MAX_ARGS]; // 명령어와 인자를 저장할 배열
    pid_t pid; // 프로세스 ID를 저장할 변수

    while (1) { // 무한 루프를 통해 쉘 동작 유지
        printf("shell> "); // 쉘 프롬프트 출력
        fflush(stdout); // 출력 버퍼를 비워 사용자 입력을 즉시 대기

        // 사용자 입력 받기
        if (!fgets(buf, sizeof(buf), stdin)) break; // EOF 또는 오류 시 종료
        buf[strcspn(buf, "\n")] = '\0'; // 줄바꿈 문자 제거

        // 빈 입력은 무시
        if (strlen(buf) == 0) continue;

        // 입력된 명령어를 공백 기준으로 분리하여 인자 배열에 저장
        int narg = getargs(buf, argv);

        // "exit" 입력 시 루프 종료 (쉘 종료)
        if (strcmp(argv[0], "exit") == 0) break;

        // 명령어 실행을 위한 자식 프로세스 생성
        pid = fork();

        if (pid == 0) { // 자식 프로세스
            // execvp로 명령어 실행
            execvp(argv[0], argv);
            perror("execvp failed"); // 실행 실패 시 오류 출력
            exit(EXIT_FAILURE);     // 실패 시 자식 프로세스 종료
        } else if (pid > 0) { // 부모 프로세스
            wait(NULL); // 자식 프로세스의 종료를 대기
        } else { // fork 실패 시 오류 처리
            perror("fork failed");
        }
    }
    return 0;
}

// 사용자 입력을 공백 단위로 분리하여 명령어와 인자를 추출
int getargs(char *cmd, char **argv) {
    int narg = 0; // 인자 개수를 저장할 변수

    while (*cmd) {
        // 공백 또는 탭 문자를 NULL로 치환하여 단어를 분리
        while (*cmd == ' ' || *cmd == '\t') *cmd++ = '\0';

        // 단어 시작 포인터 저장
        if (*cmd) argv[narg++] = cmd;

        // 다음 공백 또는 NULL 문자까지 이동
        while (*cmd && *cmd != ' ' && *cmd != '\t') cmd++;
    }

    argv[narg] = NULL; // 마지막 인자는 NULL로 종료 (execvp 요구사항)
    return narg; // 인자 개수 반환
}