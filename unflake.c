#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <wait.h>
#include <signal.h>

int killed_by_signal;
int test_pid;

void print_usage() {
    char *usage_statement = "USAGE: ./unflake max_tries max_timeout test_command args...\n"
                            "max_tries - must be greater than or equal to 1\n"
                            "max_timeout - number of seconds greater than or equal to 1\n";
    printf("%s", usage_statement);
    exit(1);
}

void print_file_contents(char *file_name) {
    FILE *test_run_output = fopen(file_name, "r");

    int c;
    while ((c = fgetc(test_run_output)) != EOF) {
        printf("%c", c);
    }

    fclose(test_run_output);
}

void print_final_output(int total_runs, char *file_name, int exit_code) {
    printf("%d runs\n", total_runs);

    if (exit_code == 255) {
        printf("killed with signal 9\n");
    } else {
        print_file_contents(file_name);
        printf("exit code %d\n", exit_code);
    }
}

void sighandler(int signum) {
    killed_by_signal = 1;
    kill(test_pid, SIGKILL);
}


int main(int argc, char **argv) {
    if (!(argc >= 4)) {
        print_usage();
    }

    int max_tries   = atoi(argv[1]);
    int max_timeout = atoi(argv[2]);

    if (max_tries == 0 || max_timeout == 0) {
        print_usage();
    }

    for (int i = 1; i <= max_tries; i++) {
        char output_file[50];
        sprintf(output_file, "test_output.%d", i);

        int fd_output = open(output_file, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

        int rc = fork();
        test_pid = rc;

        if (rc < 0) {
            fprintf(stderr, "Fork failed.\n");
        } else if (rc == 0) {
            // child process path

            dup2(fd_output, 1);  // stdout
            dup2(fd_output, 2);  // stderr

            char **newargv = &argv[3];

            char *test_command = newargv[0];
            newargv[argc] = NULL;

            execvp(test_command, newargv);
        }
        else {
            // parent process path

            int status;

            signal(SIGALRM, sighandler);
            alarm(max_timeout);

            wait(&status);

            close(fd_output);

            int exit_code;
            if (killed_by_signal) {
                if (i == max_tries) {
                    exit_code = 255;
                    print_final_output(i, output_file, exit_code);
                    exit(exit_code);
                }
            }

            if (WIFEXITED(status)) {
                exit_code = WEXITSTATUS(status);
                if (exit_code == 0 || i == max_tries) {
                    print_final_output(i, output_file, exit_code);
                    exit(exit_code);
                }
            }
        }
    }

    return 0;
}