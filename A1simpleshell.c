
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

/* Abdel Kader Gaye
NB: Some of the code was provided by the teaching assistants. */


int commandNumber = 0;
struct node *head_job = NULL;
struct node *current_job = NULL;

//Defining nodes to be used in linked lists for jobs
struct node 
{
	int number; // the job number
	int pid; // the process id of the a specific process
	struct node *next; // when another process is called you add to the end of the linked list
	char *name;
};


//Initialize the args(input) to null once the command has been processed
// this is to clear it to accept another command in the next while loop
void initialize(char *args[]);

int getcmd(char *line, char *args[], int *background);

//If there is no &, wait for the child process to finish. Else add the process to the job list
void parentProcess(int pid, char *com, int background);

//Add a job to the list of jobs
void addToJobList(char *args, int process_pid);

//Takes desired index of the linked list and returns the pid
int getPid(int index);

//Handler for Ctrl+Z
void sigstpHandler();

//Handler for Ctrl+C in order to kill all processes
void ctrl_c();


/*------------------------------------------ MAIN CODE -------------------------------------------- */
int main(void) 
{
	char *args[20];
	int bg;

	char *user = getenv("USER");
	if (user == NULL) user = "User";

	char str[sizeof(char)*strlen(user) + 4];
	sprintf(str, "\n%s>> ", user);

	//Disabling Ctrl+Z
	signal(SIGTSTP, sigstpHandler);
	//Taking care of Ctrl+C
	signal(SIGINT, ctrl_c);

	while (1) 
	{
		initialize(args);
		bg = 0;

		int length = 0;
		char *line = NULL;
		size_t linecap = 0; // 16 bit unsigned integer
		sprintf(str, "\n%s>> ", user);
		printf("%s", str);

		/*
		Reads an entire line from stream, storing the address of
		the buffer containing the text into *lineptr.  The buffer is null-
		terminated and includes the newline character, if one was found.
		check the linux manual for more info
		*/

		length = getline(&line, &linecap, stdin);
		if (length <= 0) { //if argument is empty
			exit(-1);
		}
		int cnt  = getcmd(line, args, &bg);

		//Handling empty strings as commands
		if(args[0] == NULL)
		{
			printf(" ");
		}

		else if (!strcmp("ls", args[0])) 
		{ // returns 0 if they are equal , then we negate to make the if statment true
			pid_t  pid;
			pid = fork();
			if (pid == 0)
			{ 
				execvp(args[0], args);
			}
			else 
			{
				parentProcess(pid, "ls", bg);
			}
		}

		else if (!strcmp("cd", args[0])) {

			int result = 0;
			if (args[1] == NULL) { // this will fetch home directory if you just input "cd" with no arguments
				char *home = getenv("HOME");
				if (home != NULL) {
					result = chdir(home);
				}
				else {
					printf("cd: No $HOME variable declared in the environment");
				}
			}
			//Otherwise go to specified directory
			else {
				result = chdir(args[1]);
			}
			if (result == -1) fprintf(stderr, "cd: %s: No such file or directory", args[1]);

		}

		else if (!strcmp("cat", args[0]))
		{
			if(args[1] == NULL)
					printf("cat: Missing parameters.");

			else
			{
				pid_t pid;
				pid = fork();
				
				if (pid == 0)
				{
					//Output redirection
					if(args[2] != NULL && !strcmp(">", args[2]))
					{
						//First, we open the file and clear it. If it does not exist, it gets created.
						int out = open(args[3], O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);

						close(0);
						//Replace standard output with destination file
						dup2(out, 1);
						//Printing the source file content to the destination file
						int c;
						FILE *file;
						file = fopen(args[1], "r");
						if (file) 
						{
							while ((c = getc(file)) != EOF)
							    putchar(c);
							fclose(file);
						}

						close(out);
						//Closing the redirected standard output
						close(1);
					}
					//Cat command without output redirection
					else
					{
						execvp(args[0], args);
					}
				}
			
				else
				{
					parentProcess(pid, "cat", bg);
				}
			}
		}

		else if (!strcmp("cp", args[0]))
		{
			if(args[2] == NULL)
					printf("cp: Missing destination file.");

			else 
			{
				pid_t pid;
				pid = fork();

				if (pid == 0)
				{
					execvp(args[0], args);
				}
				else
				{
					parentProcess(pid, "cp", bg);
				}
			}
		}

		else if (!strcmp("jobs", args[0]))
		{
			//Using a linked list
			struct node *ptr;
			struct node *previous;
			ptr = head_job;

			while(ptr != NULL)
			{
				int status;

				//If we are at the head node
				if(ptr == head_job)
				{
					//If the head node is dead
					if(waitpid(ptr->pid, &status, WNOHANG) == -1)
					{
						head_job = ptr->next;
						printf("%s command with job number %d is done.\n", ptr->name, ptr->number);
						free(ptr);
						ptr = head_job;
					}
					//If not dead, list it as part of the background processes
					else
					{
						printf("%s\n", ptr->name);
						previous = ptr;
						ptr = ptr->next;
					}
				}
				//If we are at any other node
				else
				{
					//If the node is dead
					if(waitpid(ptr->pid, &status, WNOHANG) == -1)
					{
						previous->next = ptr->next;
						printf("%s command with job number %d is done.\n", ptr->name, ptr->number);
						free(ptr);
						ptr = previous->next;
					}
					//If not dead, list it as part of the background processes
					else
					{
						printf("%s\n", ptr->name);
						previous = ptr;
						ptr = ptr->next;
					}
				}
			}

		}

		else if (!strcmp("fg", args[0]))
		{
			if(args[1] == NULL)
				printf("Missing job number.");
			else
			{
				printf("Switching to the process at index %s...\n", args[1]);
				pid_t newPid = 0;
				newPid = getPid(atoi(args[1]));
	
				if (newPid > 0)
				{
					printf("New foreground process\n");
					//Continue the process
					kill(newPid, SIGCONT); 
					int status;
					//Wait for the process to finish
					waitpid(newPid, &status, 0);
				}                     
			}
		} 

		else if(!strcmp("exit", args[0]))
			exit(-1);
		
		else
		{
			printf("%s is an invalid command.", args[0]);
		}

		free(line);

	}

}


/*------------------- FUNCTIONS ------------------------------- */

int getcmd(char *line, char *args[], int *background)
{
	int i = 0;
	char *token, *loc;

	//Copy the line to a new char array because after the tokenization a big part of the line gets deleted since the null pointer is moved
	char *copyCmd = malloc(sizeof(char) * sizeof(line) * strlen(line));
	sprintf(copyCmd, "%s", line);

	// Check if background is specified..
	if ((loc = index(line, '&')) != NULL) {
		*background = 1;
		*loc = ' ';
	} else
		*background = 0;

	//Create a new line pointer to solve the problem of memory leaking created by strsep() and getline() when making line = NULL
	char *lineCopy = line;
	while ((token = strsep(&lineCopy, " \t\n")) != NULL) {
		for (int j = 0; j < strlen(token); j++)
			if (token[j] <= 32)
				token[j] = '\0';
		if (strlen(token) > 0)
			args[i++] = token;
	}

	return i;
}
void initialize(char *args[]) {
	for (int i = 0; i < 20; i++) {
		args[i] = NULL;
	}
	return;
}

//If there is no &, wait for the child process to finish. Else add the process to the job list
void parentProcess(int pid, char *com, int background)
{
	if(background == 0)
	{
		int status;
		waitpid(pid, &status, WUNTRACED);
	}
	else
	{
		addToJobList(com, pid);
	}
}


//Creating a linked list for handling jobs
void addToJobList(char *args, int process_pid) 
{
	struct node *job = malloc(sizeof(struct node));

	job->name = args;

	//If the job list is empty, create a new head
	if (head_job == NULL) 
	{
		job->number = 1;
		job->pid = process_pid;

		//the new head is also the current node
		job->next = NULL;
		head_job = job;
		current_job = head_job;
	}

	//Otherwise create a new job node and link the current node to it
	else 
	{
		job->number = current_job->number + 1;
		job->pid = process_pid;

		current_job->next = job;
		current_job = job;
		job->next = NULL;
	}
}

//Takes desired index of the linked list and returns the pid
int getPid(int index)
{
    struct node *current = head_job;
    int counter = 1;
    int curr_pid = 0; 

    //Searching through the linked list for the desired node and getting its pid
    while (current != NULL)
    {
       	if (counter == index)
       	{
	        curr_pid = current->pid;
	        break;
       	}
       	counter++;
       	current = current->next;
    }
   	
    if(curr_pid == 0)
    	printf("There is no job %d.\n", index);

    return curr_pid;        
}


void sigstpHandler()
{
    printf(" ");
}

void ctrl_c()
{
	if(head_job != NULL)
	{
		printf("All processes are dead.");
		kill(head_job->pid, SIGQUIT);
		head_job = NULL;
		free(head_job);
	}
}


