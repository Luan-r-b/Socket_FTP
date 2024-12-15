#!/bin/bash

# Log file to store client messages
LOG_FILE="client_log.txt"

# Clear previous log file
> "$LOG_FILE"

# Function to run client in parallel
run_client() {
    local client_id=$1
    local parameters="arquivo_test.txt 127.0.0.1:/home/luanboschini/Documentos/UFSC/Paralela/Trabalho2/transf/test_$client_id.txt"
    arquivo_test2.txt
    (
        echo $parameters
        OUTPUT=$(./remcp "$parameters")
        echo -e "Client $client_id:\n$OUTPUT\n" >> "$LOG_FILE"
    ) &
}

# Loop to run multiple clients in parallel
for i in {1..20}; do
    run_client $i
done

# Wait for all background jobs to finish
wait

echo "All clients have finished. Check $LOG_FILE for details."