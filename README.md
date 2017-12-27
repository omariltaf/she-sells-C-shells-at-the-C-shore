# she-sells-C-shells-at-the-C-shore
A simple bash-like shell written in C.

General sytax for commands:
  command [arg1 arg2 ...] [< input_file] [> output_file] [&]
    Items in square brackets are optional.
    All words and symbols must be surrounded by spaces.

Supported Features:
  Processes can be run in the background by including an "&" character at the end of the command line.
  stdin and stdout can be redirected using "<" and ">" respectively.
  Lines beginning with a "#" character will be viewed as a comment and ignored.
  Can reference PID using "$$".
  SIGINT and SIGSTP signals can be sent to the shell's processes using Ctrl-C and Ctrl-Z respectively.
  SIGSTP signals will allow/prevent processes from being run in the background.
  Built-in custom commands: exit, cd, status.

To compile use the following line of code:
  gcc -o smallsh smallsh.c
  
