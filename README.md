# README

This project includes a server and client implementation for the command remcp, along with a test script (`test.sh`) to simulate multiple client connections. Below are the instructions to compile, run, and test the system.

## Compilation

To compile both the client and server programs, use the following commands in the terminal:

```sh
gcc -o remcp-serv remcp-serv.c
gcc -o remcp remcp.c
```
## Running the Server

To run the server in the background:

```sh
./remcp-serv &
```

## Running the Client

To run the client, is necessary to follow the pattern

```sh
./remcp [origem] [destino] 
```
One of the parameters must include the IP address of the machine where the files will be transferred.

### Example 1

```sh
./remcp arquivo_test.txt 127.0.0.1:/home/luanboschini/Documentos/UFSC/Paralela/Trabalho2/transf/test.txt
```
This command transfers a local file (arquivo_test.txt) to the specified path on the remote machine with IP 127.0.0.1.

### Example 2

```sh
./remcp 127.0.0.1:/home/luanboschini/Documentos/UFSC/Paralela/Trabalho2/arquivo_test2.txt /home/luanboschini/Documentos/UFSC/Paralela/Trabalho2/transf/test2.txt
```
This command transfers a file from the remote machine with IP 127.0.0.1 to the specified destination path in the remote machine.

## Finding and Killing the Server Process

To find the server process:

```sh
pgrep remcp-serv
```

To kill the server process (use the PID found from the previous command):

```sh
kill <PID>
```

## Giving Execution Rights to `test.sh`

To give execution rights to `test.sh`:

```sh
chmod +x test.sh
```

## Running `test.sh`

To run `test.sh`, which executes multiple clients in parallel:

```sh
./test.sh
```

## Find process using a socket

TO find a proces using a socket you go to the terminal and go:

```sh
lsof -i :<port_number>
```
