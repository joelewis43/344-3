Operating Systems - Program 3

smallsh.c
 - When run, provides user a mini command prompt

 - Runs bash commands directly
   - ls
   - pwd
   - etc...

   - Built In shell commands
   - cd
   - exit
   - status

 - Additional Functionality
   - Including & at the end of the command will launch it in the background
   - Redirects input and output like a BASH shell
   - expands $$ inside of any command/argument into shell's PID

 - Signal Handling
   - SIGINT
     - Ignored by the shell and background children
     - Will terminate any active foreground process
   - SIGTSTP
     - Toggles the background enabled mode
     - When disabled, shell will ignore &, running all processes in the foreground
