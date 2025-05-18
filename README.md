# mini_shell
Simple Unix-like shell in C supporting background jobs, pipelines, I/O redirection, and signal handling.

- Command-line editing via GNU Readline  
- Background jobs (`&`)  
- Pipelines (`|`)  
- Input (`<`) and output (`>`) redirection  
- Signal handling (ignore Ctrl+C in shell, forward to children; reap zombies)

# build & run
1.
make
./shell

2.
gcc -O3 -D_POSIX_C_SOURCE=200809 -Wall -std=c11 mini_shell.c main.c -lreadline -o shell
./shell

