#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

/*prepare calls for initialization of anything required*/
int prepare(void);
void clean(int);

/*run command*/
int process_arglist(int count, char** arglist);
void reset_interrupt(void);
int check_pipe(int count, char** arglist);
int check_background(int count, char** arglist);
int check_input(int count, char** arglist);
int check_output(int count, char** arglist);
int run_background(int count, char** arglist);
int run_pipe(int count, char** arglist);
int run_input(int count, char** arglist);
int run_output(int count, char** arglist);
int run_normal(int count, char** arglist);

/*finalize calls for destruction of anything required*/
int finalize(void);

int prepare(void)
{
	/*initialize shell's SIGINT handling behavior*/
	struct sigaction interrupt = {
		.sa_handler = SIG_IGN,
		.sa_flags = SA_RESTART
	};

	/*initialize shell's zombies handling behavior*/
	struct sigaction zombies = {
		.sa_handler = clean,
		.sa_flags = SA_RESTART | SA_NOCLDSTOP
	};

	/*send rules to kernal*/
	if (sigaction(SIGINT, &interrupt, NULL) == -1 || sigaction(SIGCHLD, &zombies, NULL) == -1)
	{
		perror("sigaction");
		return 1;
	}

	return 0;
}

/*cleans up zombie processes*/
void clean(int unused)
{
	(void)unused;
	while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

int process_arglist(int count, char** arglist)
{
	int is_background = check_background(count, arglist);
	int is_pipe = check_pipe(count, arglist);
	int is_input = check_input(count, arglist);
	int is_output = check_output(count, arglist);
	
	if (is_background){return run_background(count, arglist);}

	else if (is_pipe){return run_pipe(count, arglist);}

	else if (is_input){return run_input(count, arglist);}

	else if (is_output){return run_output(count, arglist);}

	else{return run_normal(count, arglist);}
}

/*reset child SIGINT behavior*/
void reset_interrupt(void)
{
	/*initialize child's SIGINT handling behavior to default*/
    struct sigaction interrupt = {
		.sa_handler = SIG_DFL,
		.sa_flags = SA_RESTART
	};

	/*send rule to kernal*/
    if (sigaction(SIGINT, &interrupt, NULL) == -1)
	{
		perror("sigaction");
		exit(1);
	}
}

/*check if the last argument is &*/
int check_background(int count, char** arglist)
{
	if (count < 1){return 0;}
	if (strcmp(arglist[count - 1], "&") == 0){return 1;}
	return 0;
}

/*run command in background*/
int run_background(int count, char** arglist)
{
	int pid = fork();
	arglist[count - 1] = NULL;

	/*pid error*/
    if (pid < 0) 
	{
		perror("fork");
        return 0;
    }

	/*handle child*/
    if (pid == 0) 
	{
        execvp(arglist[0], arglist);
		perror("execvp");
        exit(1);
    }

	/*handle parent*/
    else{return 1;}
}

/*check if the command has a pipe*/
int check_pipe(int count, char** arglist)
{
	int i;
	for (i = 0; i < count; ++i)
	{
		if (strcmp(arglist[i], "|") == 0){return 1;}
	}
	return 0;
}

/*run command with pipe*/
int run_pipe(int count, char** arglist)
{
	int i, j, num = 0;
    int positions[10];
    int fds[9][2];
    int idx_start = 0, idx_end;
    int pid, children[10];

	/*save pipes positions*/
	for (i = 0; i < count; i++)
	{
		if (strcmp(arglist[i], "|") == 0){positions[num++] = i;}
	}
	positions[num] = count;
	
	/*create pipes*/
	for (i = 0; i < num; i++)
	{
        if (pipe(fds[i]) < 0)
		{
            perror("pipe");
            return 0;
        }
    }

	for (i = 0; i <= num; i++)
	{
		idx_end = positions[i];
		pid = fork();

		/*pid error*/
		if (pid < 0)
		{
			perror("fork");
			return 0;
		}

		/*handle child*/
		if (pid == 0)
		{	
			reset_interrupt();

			/*if not first, read from prev*/
			if (i > 0)
			{
                if (dup2(fds[i-1][0], 0) < 0)
				{
                    perror("dup2");
                    exit(1);
                }
        	}

			/*if not last, write to next*/
			if (i < num)
			{
				if (dup2(fds[i][1], 1) < 0)
				{
					perror("dup2");
					exit(1);
				}
			}

			/*close all the file descriptors*/
			for (j = 0; j < num; j++)
			{
				close(fds[j][0]);
				close(fds[j][1]);
			}

			/*execute command*/
			arglist[idx_end] = NULL;
			execvp(arglist[idx_start], &arglist[idx_start]);
			perror("execvp");
			exit(1);
		}

		/*handle parent*/
		else
		{
			idx_start = idx_end + 1;
			children[i] = pid;
		}
	}

	/*close all the file descriptors*/
	for (i = 0; i < num; i++)
	{
		close(fds[i][0]);
		close(fds[i][1]);
	}

	/*wait for all children*/
	for (i = 0; i <= num; i++)
	{
		while (waitpid(children[i], NULL, 0) == -1)
		{
			if (errno == EINTR){continue;}
			if (errno == ECHILD){break;}
			perror("waitpid");
			return 0;
		}
	}

	return 1;
}

/*check if the command has input redirection*/
int check_input(int count, char** arglist)
{
	if (count < 2){return 0;}
	if (strcmp(arglist[count - 2], "<") == 0){return 1;}
	return 0;
}

/*run command with input redirection*/
int run_input(int count, char** arglist)
{
	int pid = fork(), fd;

	/*pid error*/
	if (pid < 0)
	{
		perror("fork");
		return 0;
	}

	/*handle child*/
	if (pid == 0)
	{
		reset_interrupt();

		fd = open(arglist[count - 1], O_RDONLY);
		if (fd < 0) 
		{
			perror("open");
			exit(1);
		}
		if (dup2(fd, 0) < 0) 
		{
			perror("dup2");
			close(fd);
			exit(1);
		}
		close(fd);

		arglist[count - 2] = NULL;
		execvp(arglist[0], arglist);
		perror("execvp");
		exit(1);
	}

	/*wait for child*/
	else
	{
		while (waitpid(pid, NULL, 0) == -1)
		{
			if (errno == EINTR){continue;}
    		if (errno == ECHILD){break;}
			perror("waitpid");
			return 0;
		}
		return 1;
	}
}

/*check if the command has output redirection*/
int check_output(int count, char** arglist)
{
	if (count < 2){return 0;}
	if (strcmp(arglist[count - 2], ">") == 0){return 1;}
	return 0;
}

/*run command with output redirection*/
int run_output(int count, char** arglist)
{
	int pid = fork(), fd;

	/*pid error*/
	if (pid < 0)
	{
		perror("fork");
		return 0;
	}

	/*handle child*/
	if (pid == 0)
	{
		reset_interrupt();

		fd = open(arglist[count - 1], O_WRONLY | O_CREAT | O_TRUNC, 0600);
		if (fd < 0) 
		{
			perror("open");
			exit(1);
		}
		if (dup2(fd, 1) < 0) 
		{
			perror("dup2");
			close(fd);
			exit(1);
		}
		close(fd);

		arglist[count - 2] = NULL;
		execvp(arglist[0], arglist);
		perror("execvp");
		exit(1);
	}

	/*wait for child*/
	else
	{
		while (waitpid(pid, NULL, 0) == -1)
		{
			if (errno == EINTR){continue;}
    		if (errno == ECHILD){break;}
			perror("waitpid");
			return 0;
		}
		return 1;
	}
}

/*run command normally*/
int run_normal(int count, char** arglist)
{
    int pid = fork();

	/*pid error*/
    if (pid < 0) 
	{
		perror("fork");
        return 0;
    }

	/*handle child*/
    if (pid == 0) 
	{
        reset_interrupt();

        execvp(arglist[0], arglist);
		perror("execvp");
        exit(1); //todo
    }

	/*wait for child*/
    else
	{
		while (waitpid(pid, NULL, 0) == -1)
		{
			if (errno == EINTR){continue;}
    		if (errno == ECHILD){break;}
			perror("waitpid");
			return 0;
		}
		return 1;
	}
}

int finalize(void)
{
    return 0;
}
