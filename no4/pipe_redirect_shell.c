#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE 256 // 최대 입력 줄 길이
#define MAX_ARGS 50  // 최대 명령어 인자 수

int getargs(char *cmd, char **argv);

int main() {
    char buf[MAX_LINE]; // 입력 저장 버퍼
    char *argv[MAX_ARGS]; // 명령어와 인자 배열
    pid_t pid;

    while (1) {
        printf("shell> ");
        fflush(stdout);

        if (!fgets(buf, sizeof(buf), stdin)) break; // 입력 받기, EOF 처리
        buf[strcspn(buf, "\n")] = '\0'; // 줄바꿈 제거

        if (strlen(buf) == 0) continue; // 빈 입력 무시

        // 파이프 처리
        char *commands[2];
        int is_pipe = 0;
        if ((commands[0] = strtok(buf, "|")) && (commands[1] = strtok(NULL, "|"))) {
            is_pipe = 1; // 파이프 명령어인지 확인
        }

        if (!is_pipe) { // 단일 명령어 처리
            int narg = getargs(commands[0], argv);
            if (strcmp(argv[0], "exit") == 0) break; // exit 처리

            pid = fork();
            if (pid == 0) { // 자식 프로세스
                for (int i = 0; i < narg; i++) { // 파일 재지향 처리
                    if (strcmp(argv[i], ">") == 0) {
                        int fd = open(argv[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        if (fd < 0) perror("open failed");
                        dup2(fd, STDOUT_FILENO);
                        close(fd);
                        argv[i] = NULL;
                        break;
                    } else if (strcmp(argv[i], "<") == 0) {
                        int fd = open(argv[i + 1], O_RDONLY);
                        if (fd < 0) perror("open failed");
                        dup2(fd, STDIN_FILENO);
                        close(fd);
                        argv[i] = NULL;
                        break;
                    }
                }
                execvp(argv[0], argv); // 명령어 실행
                perror("execvp failed");
                exit(EXIT_FAILURE);
            } else if (pid > 0) {
                wait(NULL); // 자식 프로세스 종료 대기
            } else {
                perror("fork failed");
            }
        } else { // 파이프 처리
            int pipe_fd[2];
            if (pipe(pipe_fd) == -1) {
                perror("pipe failed");
                continue;
            }

            pid_t pid1 = fork();
            if (pid1 == 0) { // 첫 번째 명령어 실행
                close(pipe_fd[0]);
                dup2(pipe_fd[1], STDOUT_FILENO);
                close(pipe_fd[1]);
                char *argv1[MAX_ARGS];
                getargs(commands[0], argv1);
                execvp(argv1[0], argv1);
                perror("execvp failed");
                exit(EXIT_FAILURE);
            }

            pid_t pid2 = fork();
            if (pid2 == 0) { // 두 번째 명령어 실행
                close(pipe_fd[1]);
                dup2(pipe_fd[0], STDIN_FILENO);
                close(pipe_fd[0]);
                char *argv2[MAX_ARGS];
                getargs(commands[1], argv2);
                execvp(argv2[0], argv2);
                perror("execvp failed");
                exit(EXIT_FAILURE);
            }

            close(pipe_fd[0]);
            close(pipe_fd[1]);
            wait(NULL); // 첫 번째 명령어 종료 대기
            wait(NULL); // 두 번째 명령어 종료 대기
        }
    }
    return 0;
}

// 명령어와 인자를 공백 단위로 분리
int getargs(char *cmd, char **argv) {
    int narg = 0;
    while (*cmd) {
        while (*cmd == ' ' || *cmd == '\t') *cmd++ = '\0'; // 공백 제거
        if (*cmd) argv[narg++] = cmd;
        while (*cmd && *cmd != ' ' && *cmd != '\t') cmd++;
    }
    argv[narg] = NULL; // 마지막 인자 NULL
    return narg;
}