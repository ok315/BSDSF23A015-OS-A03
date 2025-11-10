/* src/jobs.c
 * Job list manager with reaper notifications (defines reap_zombies)
 */

#include "shell.h"
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>

static job_t jobs[MAX_JOBS];
static int job_count = 0;

/* Add a background job (store pid and command string)
   Returns 1-based job index on success, -1 on failure.
*/
int add_job(pid_t pid, const char *cmd) {
    if (job_count >= MAX_JOBS) {
        fprintf(stderr, "jobs: job list full, cannot add pid %d\n", (int)pid);
        return -1;
    }
    jobs[job_count].pid = pid;
    strncpy(jobs[job_count].cmd, cmd ? cmd : "", JOB_CMD_LEN - 1);
    jobs[job_count].cmd[JOB_CMD_LEN - 1] = '\0';
    job_count++;
    /* return 1-based job number */
    return job_count;
}

/* Remove job by pid (shifts array) */
void remove_job(pid_t pid) {
    int found = -1;
    for (int i = 0; i < job_count; ++i) {
        if (jobs[i].pid == pid) {
            found = i;
            break;
        }
    }
    if (found == -1) return;
    for (int j = found; j < job_count - 1; ++j) jobs[j] = jobs[j + 1];
    job_count--;
}

/* Print active jobs with index-based job numbers */
void print_jobs(void) {
    for (int i = 0; i < job_count; ++i) {
        printf("[%d] %d %s\n", i + 1, (int)jobs[i].pid, jobs[i].cmd);
    }
}

/* Reap any finished background children without blocking and notify user.
   This function name matches what main.c calls: reap_zombies(). */
void reap_zombies(void) {
    int status;
    pid_t pid;
    /* Loop: multiple children may have terminated */
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        /* find the job entry for this pid so we can show the command when notifying */
        char saved_cmd[JOB_CMD_LEN] = {0};
        int found = -1;
        for (int i = 0; i < job_count; ++i) {
            if (jobs[i].pid == pid) {
                strncpy(saved_cmd, jobs[i].cmd, JOB_CMD_LEN - 1);
                found = i;
                break;
            }
        }

        if (found != -1) {
            int jobnum = found + 1; /* save job number for notification */

            /* Remove the job entry from list */
            remove_job(pid);

            /* Print a short notification (we call this before printing prompt) */
            if (WIFEXITED(status)) {
                int exitcode = WEXITSTATUS(status);
                printf("\n[%d] Done    %s (exit %d)\n", jobnum, saved_cmd, exitcode);
            } else if (WIFSIGNALED(status)) {
                int sig = WTERMSIG(status);
                printf("\n[%d] Killed  %s (signal %d)\n", jobnum, saved_cmd, sig);
            } else {
                printf("\n[%d] Finished %s\n", jobnum, saved_cmd);
            }
            fflush(stdout);
        } else {
            /* Job not in list, still reaped - ignore or optionally log */
        }
    }
    /* if pid == 0 => no child exited; if pid == -1 handle errno */
    if (pid == -1 && errno != ECHILD) {
        /* Unexpected error from waitpid */
        perror("waitpid");
    }
}
