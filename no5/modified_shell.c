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

#define MAX_LINE 256       // 최대 명령어 입력 길이
#define MAX_ARGS 50        // 최대 명령어 인자 수

// 사용자 입력 명령어를 공백 단위로 분리
int getargs(char *cmd, char **argv);

// 내장 명령어 처리 함수(ls, pwd, cd, mkdir 등)
int handle_builtin_commands(char **argv);

// 외부 명령어 실행 함수
void execute_command(char **argv);

int main() {
    char buf[MAX_LINE];    // 사용자 입력 버퍼
    char *argv[MAX_ARGS];  // 명령어 및 인자 배열

    while (1) {            // 쉘 실행 루프
        printf("shell> "); // 프롬프트 출력
        fflush(stdout);    // 출력 버퍼 플러시

        if (!fgets(buf, sizeof(buf), stdin)) break; // 사용자 입력 받기 (EOF 처리)
        buf[strcspn(buf, "\n")] = '\0';             // 줄바꿈 문자 제거

        if (strlen(buf) == 0) continue;             // 빈 입력 무시

        int narg = getargs(buf, argv);              // 명령어와 인자 분리

        if (strcmp(argv[0], "exit") == 0) break;    // "exit" 명령어로 쉘 종료
        
        if (handle_builtin_commands(argv) == 0) {   // 내장 명령어 처리
            continue; // 내장 명령어가 처리되었으면 다음 명령어로 이동
        }

        execute_command(argv); // 외부 명령어 실행
    }
    return 0; // 쉘 종료
}

// 사용자 입력을 공백 단위로 분리하여 명령어와 인자를 추출
int getargs(char *cmd, char **argv) {
    int narg = 0;
    while (*cmd) {
        while (*cmd == ' ' || *cmd == '\t') *cmd++ = '\0'; // 공백 제거
        if (*cmd) argv[narg++] = cmd;                      // 명령어 또는 인자 저장
        while (*cmd && *cmd != ' ' && *cmd != '\t') cmd++; // 다음 공백까지 이동
    }
    argv[narg] = NULL; // 마지막 인자 NULL로 설정
    return narg;
}

// 내장 명령어 처리 함수
int handle_builtin_commands(char **argv) {
    // "ls" 명령어 구현
    if (strcmp(argv[0], "ls") == 0) { 
        DIR *dir;
        struct dirent *entry;

        dir = opendir("."); // 현재 디렉토리 열기
        if (dir == NULL) {
            perror("opendir failed"); // 디렉토리 열기 실패 시 에러 출력
            return 0;
        }

        while ((entry = readdir(dir)) != NULL) { // 디렉토리 엔트리 읽기
            printf("%s\n", entry->d_name);      // 파일/디렉토리 이름 출력
        }
        closedir(dir); // 디렉토리 닫기
        return 0; // 처리 완료
    }

    // "pwd" 명령어 구현
    if (strcmp(argv[0], "pwd") == 0) {
        char cwd[MAX_LINE];
        if (getcwd(cwd, sizeof(cwd))) { // 현재 작업 디렉토리 가져오기
            printf("%s\n", cwd);        // 디렉토리 경로 출력
        } else {
            perror("getcwd failed");   // 실패 시 에러 출력
        }
        return 0; // 처리 완료
    }

    // "cd" 명령어 구현
    if (strcmp(argv[0], "cd") == 0) { 
        if (argv[1] == NULL) {
            fprintf(stderr, "Usage: cd <directory>\n"); // 인자 누락 시 오류 출력
        } else {
            if (chdir(argv[1]) == -1) { // 디렉토리 변경
                perror("chdir failed"); // 실패 시 에러 출력
            }
        }
        return 0; // 처리 완료
    }

    // "mkdir" 명령어 구현
    if (strcmp(argv[0], "mkdir") == 0) { 
        if (argv[1] == NULL) {
            fprintf(stderr, "Usage: mkdir <directory>\n"); // 인자 누락 시 오류 출력
        } else {
            if (mkdir(argv[1], 0755) == -1) { // 디렉토리 생성
                perror("mkdir failed");       // 실패 시 에러 출력
            }
        }
        return 0; // 처리 완료
    }

    // "rmdir" 명령어 구현
    if (strcmp(argv[0], "rmdir") == 0) {
        if (argv[1] == NULL) {
            fprintf(stderr, "rmdir: missing operand\n"); // 인자 누락 시 오류 출력
        } else if (rmdir(argv[1]) != 0) { // 디렉토리 제거
            perror("rmdir failed");       // 실패 시 에러 출력
        }
        return 0; // 처리 완료
    }

    // "ln" 명령어 구현 (하드 링크 및 심볼릭 링크)
    if (strcmp(argv[0], "ln") == 0) {
        int symbolic = 0; // 심볼릭 링크 여부 플래그
        if (argv[1] != NULL && strcmp(argv[1], "-s") == 0) {
            symbolic = 1; // 심볼릭 링크 플래그 설정
            argv++;       // 옵션 제거
        }
        if (argv[1] == NULL || argv[2] == NULL) {
            fprintf(stderr, "ln: missing operand\n");
        } else if (symbolic) {
            if (symlink(argv[1], argv[2]) != 0) { // 심볼릭 링크 생성
                perror("ln (symbolic)");
            }
        } else {
            if (link(argv[1], argv[2]) != 0) { // 하드 링크 생성
                perror("ln (hard)");
            }
        }
        return 0; // 처리 완료
    }

    // "cp" 명령어 구현
    if (strcmp(argv[0], "cp") == 0) {
        if (argv[1] == NULL || argv[2] == NULL) {
            fprintf(stderr, "cp: missing operand\n");
        } else {
            int src_fd = open(argv[1], O_RDONLY);
            if (src_fd < 0) {
                perror("cp: open source");
                return 0;
            }
            int dest_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (dest_fd < 0) {
                perror("cp: open destination");
                close(src_fd);
                return 0;
            }

            char buffer[4096];
            ssize_t bytes;
            while ((bytes = read(src_fd, buffer, sizeof(buffer))) > 0) {
                if (write(dest_fd, buffer, bytes) != bytes) {
                    perror("cp: write");
                    close(src_fd);
                    close(dest_fd);
                    return 0;
                }
            }

            close(src_fd);
            close(dest_fd);

            if (bytes < 0) {
                perror("cp: read");
            }
        }
        return 0; // 처리 완료
    }

    // rm 명령어 구현
    if (strcmp(argv[0], "rm") == 0) {
        if (argv[1] == NULL) {
            fprintf(stderr, "rm: missing operand\n");
            return 0;
        }
        if (unlink(argv[1]) != 0) {
            perror("rm");
        }
        return 0;
    }

    // mv 명령어 구현
    if (strcmp(argv[0], "mv") == 0) {
        if (argv[1] == NULL || argv[2] == NULL) {
            fprintf(stderr, "mv: missing operand\n");
            return 0;
        }
        if (rename(argv[1], argv[2]) != 0) {
            perror("mv");
        }
        return 0;
    }

    // cat 명령어 구현
    if (strcmp(argv[0], "cat") == 0) {
        if (argv[1] == NULL) {
            fprintf(stderr, "cat: missing operand\n");
            return 0;
        }
        for (int i = 1; argv[i] != NULL; i++) {
            int fd = open(argv[i], O_RDONLY);
            if (fd < 0) {
                perror("cat");
                continue;
            }
            char buffer[1024];
            ssize_t bytes;
            while ((bytes = read(fd, buffer, sizeof(buffer))) > 0) {
                write(STDOUT_FILENO, buffer, bytes);
            }
            close(fd);
        }
        return 0;
    }
    
    // 기타 명령어는 처리되지 않음
    return 1;
}

// 외부 명령어 실행 함수
void execute_command(char **argv) {
    pid_t pid = fork(); // 자식 프로세스 생성
    if (pid == 0) { // 자식 프로세스
        execvp(argv[0], argv); // 외부 명령어 실행
        perror("exec failed"); // 실패 시 에러 출력
        exit(1);
    } else if (pid > 0) { // 부모 프로세스
        waitpid(pid, NULL, 0); // 자식 프로세스 종료 대기
    } else {
        perror("fork failed"); // fork 실패 시 에러 출력
    }
}