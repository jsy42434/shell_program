#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_LINE 256 // 최대 명령어 길이
#define MAX_ARGS 50  // 최대 명령어 인자 수

// 함수 선언부
int getargs(char *cmd, char **argv);  // 입력 명령어를 공백으로 분리하는 함수
int handle_builtin_commands(char **argv);  // 내장 명령어를 처리하는 함수
void execute_external_command(char **argv); // 외부 명령어를 실행하는 함수
void handle_sigint(int sig); // SIGINT(Ctrl-C) 처리
void handle_sigquit(int sig);  // SIGQUIT 처리
void handle_sigtstp(int sig);  // SIGTSTP(Ctrl-Z) 처리

int main() {
    char buf[MAX_LINE];       // 사용자 입력을 저장할 버퍼
    char *argv[MAX_ARGS];     // 명령어와 인자를 저장할 배열
    pid_t pid;                // 프로세스 ID를 저장할 변수

    // 시그널 핸들러 등록
    signal(SIGINT, handle_sigint);
    signal(SIGQUIT, handle_sigquit);
    signal(SIGTSTP, handle_sigtstp);

    while (1) {
        // 프롬프트 출력
        printf("shell> ");
        fflush(stdout);

        // 사용자 입력 받기
        if (!fgets(buf, sizeof(buf), stdin)) break; 
        buf[strcspn(buf, "\n")] = '\0'; // 개행 문자 제거

        if (strlen(buf) == 0) continue; // 빈 입력은 무시

        char *commands[2];
        int is_pipe = 0; // 파이프 여부를 확인하는 플래그
        if ((commands[0] = strtok(buf, "|")) && (commands[1] = strtok(NULL, "|"))) {
            is_pipe = 1; // 파이프 명령어일 경우 플래그 설정
        }

        if (!is_pipe) { // 파이프가 없는 단일 명령어 처리
            int narg = getargs(commands[0], argv); // 명령어와 인자를 분리
            
            // '&' 처리: 명령어 끝에 &가 있는지 확인
            int background = 0; // 백그라운드 실행 여부 플래그
            for (int i = 0; argv[i] != NULL; i++) {
                if (strcmp(argv[i], "&") == 0) { // '&' 발견 시
                    background = 1; // 백그라운드 실행 플래그 설정
                    argv[i] = NULL; // '&'를 제거하여 실행 명령어에서 제외
                    break;
                }
            }

            if (strcmp(argv[0], "exit") == 0) break; // "exit" 입력 시 프로그램 종료

            if (handle_builtin_commands(argv) == 0) continue; // 내장 명령어 처리

            // 자식 프로세스 생성
            pid = fork();
            if (pid == 0) { // 자식 프로세스
                // 파일 리다이렉션 처리
                for (int i = 0; argv[i] != NULL; i++) {
                    if (strcmp(argv[i], ">") == 0) { // 출력 리다이렉션
                        int fd = open(argv[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        if (fd < 0) {
                            perror("Failed to open output file");
                            exit(EXIT_FAILURE);
                        }
                        dup2(fd, STDOUT_FILENO);
                        close(fd);
                        argv[i] = NULL;
                        break;
                    } else if (strcmp(argv[i], "<") == 0) { // 입력 리다이렉션
                        int fd = open(argv[i + 1], O_RDONLY);
                        if (fd < 0) {
                            perror("Failed to open input file");
                            exit(EXIT_FAILURE);
                        }
                        dup2(fd, STDIN_FILENO);
                        close(fd);
                        argv[i] = NULL;
                        break;
                    }
                }
                execute_external_command(argv); // 외부 명령어 실행
                exit(EXIT_FAILURE);
            } else if (pid > 0) { // 부모 프로세스
                if (!background) { // 백그라운드가 아니면 대기
                    waitpid(pid, NULL, 0); // 자식 프로세스 종료 대기
                } else { // 백그라운드 실행
                    printf("[Process running in background with PID %d]\n", pid);
                }
            } else {
                perror("fork failed"); // fork 실패 시 에러 메시지 출력
            }
        } else { // 파이프 처리
            int pipe_fd[2]; // 파이프 파일 디스크립터
            if (pipe(pipe_fd) == -1) {
                perror("pipe failed");
                continue;
            }

            pid_t pid1 = fork();
            if (pid1 == 0) { // 첫 번째 명령어 실행
                close(pipe_fd[0]); // 읽기 끝 닫기
                dup2(pipe_fd[1], STDOUT_FILENO); // 표준 출력 파이프 연결
                close(pipe_fd[1]);
                char *argv1[MAX_ARGS];
                getargs(commands[0], argv1);
                execute_external_command(argv1); // 외부 명령어 실행
                exit(EXIT_FAILURE);
            }

            pid_t pid2 = fork();
            if (pid2 == 0) { // 두 번째 명령어 실행
                close(pipe_fd[1]); // 쓰기 끝 닫기
                dup2(pipe_fd[0], STDIN_FILENO); // 표준 입력 파이프 연결
                close(pipe_fd[0]);
                char *argv2[MAX_ARGS];
                getargs(commands[1], argv2);
                execute_external_command(argv2); // 외부 명령어 실행
                exit(EXIT_FAILURE);
            }

            // 부모 프로세스에서 파이프 닫기
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            wait(NULL); // 첫 번째 자식 프로세스 종료 대기
            wait(NULL); // 두 번째 자식 프로세스 종료 대기
        }
    }
    return 0; // 프로그램 종료
}

// 명령어를 공백으로 구분
int getargs(char *cmd, char **argv) {
    int narg = 0;
    while (*cmd) {
        while (*cmd == ' ' || *cmd == '\t') *cmd++ = '\0';
        if (*cmd) argv[narg++] = cmd;
        while (*cmd && *cmd != ' ' && *cmd != '\t') cmd++;
    }
    argv[narg] = NULL;
    return narg;
}

// 내장 명령어 처리 함수
int handle_builtin_commands(char **argv) {
    // ls 명령어: 현재 디렉토리의 파일 및 디렉토리 목록 출력
    if (strcmp(argv[0], "ls") == 0) {
        DIR *dir;
        struct dirent *entry;

        dir = opendir("."); // 현재 디렉토리를 열기
        if (dir == NULL) { // 디렉토리 열기에 실패한 경우 오류 출력
            perror("opendir failed");
            return 0;
        }
        while ((entry = readdir(dir)) != NULL) { // 디렉토리 엔트리 읽기
            printf("%s\n", entry->d_name); // 엔트리 이름 출력
        }
        closedir(dir); // 디렉토리 닫기
        return 0;
    }

    // pwd 명령어: 현재 작업 디렉토리 출력
    if (strcmp(argv[0], "pwd") == 0) {
        char cwd[MAX_LINE];
        if (getcwd(cwd, sizeof(cwd))) { // 현재 디렉토리 경로를 가져오기
            printf("%s\n", cwd); // 디렉토리 경로 출력
        } else { // 경로 가져오기에 실패한 경우 오류 출력
            perror("getcwd failed");
        }
        return 0;
    }

    // cd 명령어: 현재 작업 디렉토리 변경
    if (strcmp(argv[0], "cd") == 0) {
        if (argv[1] == NULL) { // 디렉토리 인자가 없는 경우 사용법 출력
            fprintf(stderr, "Usage: cd <directory>\n");
        } else if (chdir(argv[1]) == -1) { // 디렉토리 변경에 실패한 경우 오류 출력
            perror("chdir failed");
        }
        return 0;
    }

    // mkdir 명령어: 새로운 디렉토리 생성
    if (strcmp(argv[0], "mkdir") == 0) {
        if (argv[1] == NULL) { // 디렉토리 이름이 제공되지 않은 경우 사용법 출력
            fprintf(stderr, "Usage: mkdir <directory>\n");
        } else if (mkdir(argv[1], 0755) == -1) { // 디렉토리 생성에 실패한 경우 오류 출력
            perror("mkdir failed");
        }
        return 0;
    }

    // rmdir 명령어: 빈 디렉토리 제거
    if (strcmp(argv[0], "rmdir") == 0) {
        if (argv[1] == NULL) { // 디렉토리 이름이 제공되지 않은 경우 사용법 출력
            fprintf(stderr, "rmdir: missing operand\n");
        } else if (rmdir(argv[1]) != 0) { // 디렉토리 제거에 실패한 경우 오류 출력
            perror("rmdir failed");
        }
        return 0;
    }

    return 1; // 해당 명령어가 처리되지 않았음을 반환
}

// 파일 작업 명령어
int execute_command_direct(char **argv) {
    // cat 명령어: 파일 내용을 표준 출력으로 출력
    if (strcmp(argv[0], "cat") == 0) {
        if (argv[1] == NULL) { // 파일 이름이 제공되지 않은 경우 오류 출력
            fprintf(stderr, "cat: missing operand\n");
            return 0;
        }
        for (int i = 1; argv[i] != NULL; i++) {
            int fd = open(argv[i], O_RDONLY); // 파일 읽기 모드로 열기
            if (fd < 0) { // 파일 열기에 실패한 경우 오류 출력
                perror("cat");
                continue;
            }
            char buffer[1024];
            ssize_t bytes;
            while ((bytes = read(fd, buffer, sizeof(buffer))) > 0) { // 파일 내용 읽기
                write(STDOUT_FILENO, buffer, bytes); // 표준 출력으로 쓰기
            }
            close(fd); // 파일 닫기
        }
        return 0;
    }

    // cp 명령어: 파일 복사
    if (strcmp(argv[0], "cp") == 0) {
        if (argv[1] == NULL || argv[2] == NULL) { // 원본 파일 또는 대상 파일 이름이 제공되지 않은 경우 오류 출력
            fprintf(stderr, "cp: missing operand\n");
            return 0;
        }
        int src_fd = open(argv[1], O_RDONLY); // 원본 파일 열기
        if (src_fd < 0) {
            perror("cp: source file");
            return 0;
        }
        int dest_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644); // 대상 파일 열기 또는 생성
        if (dest_fd < 0) {
            perror("cp: destination file");
            close(src_fd);
            return 0;
        }

        char buffer[4096];
        ssize_t bytes;
        while ((bytes = read(src_fd, buffer, sizeof(buffer))) > 0) { // 원본 파일 읽기
            write(dest_fd, buffer, bytes); // 대상 파일에 쓰기
        }

        close(src_fd); // 원본 파일 닫기
        close(dest_fd); // 대상 파일 닫기
        return 0;
    }

    // rm 명령어: 파일 삭제
    if (strcmp(argv[0], "rm") == 0) {
        if (argv[1] == NULL) { // 파일 이름이 제공되지 않은 경우 오류 출력
            fprintf(stderr, "rm: missing operand\n");
            return 0;
        }
        if (unlink(argv[1]) != 0) { // 파일 삭제
            perror("rm");
        }
        return 0;
    }

    // mv 명령어: 파일 이동 또는 이름 변경
    if (strcmp(argv[0], "mv") == 0) {
        if (argv[1] == NULL || argv[2] == NULL) { // 원본 파일 또는 대상 파일 이름이 제공되지 않은 경우 오류 출력
            fprintf(stderr, "mv: missing operand\n");
            return 0;
        }
        if (rename(argv[1], argv[2]) != 0) { // 파일 이름 변경 또는 이동
            perror("mv");
        }
        return 0;
    }

    // ln 명령어: 하드 링크 생성
    if (strcmp(argv[0], "ln") == 0) {
        if (argv[1] == NULL || argv[2] == NULL) { // 원본 파일 또는 링크 이름이 제공되지 않은 경우 오류 출력
            fprintf(stderr, "ln: missing operand\n");
            return 0;
        }
        if (link(argv[1], argv[2]) != 0) { // 하드 링크 생성
            perror("ln");
        }
        return 0;
    }

    return 1; // 명령어를 처리하지 못한 경우
}

// 외부 명령어 실행 함수
void execute_external_command(char **argv) {
    char path[1024];
    snprintf(path, sizeof(path), "/bin/%s", argv[0]);
    if (access(path, X_OK) == 0) {
        execv(path, argv);
    } else {
        snprintf(path, sizeof(path), "/usr/bin/%s", argv[0]);
        if (access(path, X_OK) == 0) {
            execv(path, argv);
        } else {
            fprintf(stderr, "Unknown command: %s\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}

// 시그널 핸들러: SIGINT
void handle_sigint(int sig) {
    printf("\nCaught signal %d (SIGINT). Exiting gracefully...\n", sig);
    exit(0);
}

// 시그널 핸들러: SIGQUIT
void handle_sigquit(int sig) {
    printf("\nCaught signal %d (SIGQUIT). Ignoring and resuming...\n", sig);
}

// 시그널 핸들러: SIGTSTP
void handle_sigtstp(int sig) {
    printf("\nCaught signal %d (SIGTSTP). Stopping is disabled. Resuming...\n", sig);
}