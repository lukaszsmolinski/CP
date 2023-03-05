# CP
This repository contains my solutions to assignments for Concurrent Programming course at the University of Warsaw (MIMUW). 

## executor

This project implements an executor program that can run multiple background processes and interact with them through a set of commands.

### Features:
- Ability to run a new task in the background with run command.
- Retrieving the last line of standard output of a task with out command.
- Retrieving the last line of standard error output of a task with err command.
- Sending a SIGINT signal to a task to interrupt it with kill command.
- Supporting sleep command to pause the executor for a specified duration.
- Shutdown of the executor and all tasks with quit command.
- Printing a message when a task ends.

### How to use

```
mkdir build && cd build
cmake ..
make
./executor
```

Once the executor is started, it will read commands from the standard input and execute them. The supported commands are:
- ```run A B C ...``` - Creates a new background task by executing the program A with the given arguments B C .... The task is identified by a unique task ID, and the executor prints a message of the form ```Task T started: pid P```. where T is the task ID and P is the process ID of the task.
- ```out T``` - Prints the last line of standard output of task T. The output is printed in the form ```Task T stdout: 'S'```, where T is the task ID and S is the last line of output.
- ```err T``` - Prints the last line of standard error output of task T. The output is printed in the form ```Task T stderr: 'S'```, where T is the task ID and S is the last line of output.
- ```kill T``` - Sends a SIGINT signal to task T to interrupt it. If the program has already terminated, the signal is ignored.
- ```sleep N``` - Pauses the executor for N milliseconds.
- ```quit``` or EOF - Exits the executor, terminating all running tasks.


## workshop

This is a Java program that simulates workshops with multiple workstations. Each workstation has a unique identifier and can be used by any user. Users are represented by Java threads and can enter the workshop, occupy a workstation, and switch between workstations using the Workshop interface methods provided. The program can handle multiple independent workshops with disjoint sets of workstations. Users can occupy workstations indefinitely and can enter and leave the workshop at any time. The program provides thread-safe coordination of user access to workstations.
