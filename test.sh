#!/bin/bash

set -e  # Exit on any error

# Run the program with 3 processes and initial balances $10, $20, $30
# Capture stdout to a file for validation while also displaying it
./run.sh -p 3 10 20 30 2>&1 | tee output.log

# Number of child processes (process 0 is parent)
NUM_PROCESSES=3
TOTAL_PROCESSES=$((NUM_PROCESSES + 1))  # Including parent process 0

echo "Validating events.log for $NUM_PROCESSES child processes (processes 1-$NUM_PROCESSES)..."

# Validate that events.log exists
if [ ! -f events.log ]; then
    echo "ERROR: events.log not found"
    exit 1
fi

# Check for each process (0 is parent, 1..N are children)
for ((i=0; i<TOTAL_PROCESSES; i++)); do
    echo "Checking process $i..."

    # All processes should receive all STARTED messages
    # Format: "%d: process %1d received all STARTED messages\n"
    if ! grep -qE "^[0-9]+: process $i received all STARTED messages$" events.log; then
        echo "ERROR: Process $i did not receive all STARTED messages"
        exit 1
    fi

    # All processes should receive all DONE messages
    # Format: "%d: process %1d received all DONE messages\n"
    if ! grep -qE "^[0-9]+: process $i received all DONE messages$" events.log; then
        echo "ERROR: Process $i did not receive all DONE messages"
        exit 1
    fi

    # Only child processes (type C) should have DONE their work
    # Parent process (type K, id=0) does not do this
    if [ $i -ne 0 ]; then
        # Format: "%d: process %1d has DONE with balance $%2d\n"
        if ! grep -qE "^[0-9]+: process $i has DONE with balance \\\$[0-9]+$" events.log; then
            echo "ERROR: Process $i did not log DONE with balance"
            exit 1
        fi

        # Each child process should have STARTED
        # Format: "%d: process %1d (pid %5d, parent %5d) has STARTED with balance $%2d\n"
        if ! grep -qE "^[0-9]+: process $i \(pid [0-9]+, parent [0-9]+\) has STARTED with balance \\\$[0-9]+$" events.log; then
            echo "ERROR: Process $i did not log STARTED with balance"
            exit 1
        fi
    fi

    echo "  ✓ Process $i validated"
done

echo ""
echo "Validating balance history output..."

# Check that balance history table was printed to stdout
if ! grep -q "Full balance history" output.log; then
    echo "ERROR: Balance history table not found in output.log"
    exit 1
fi

# Validate the table contains process rows
for ((i=1; i<=NUM_PROCESSES; i++)); do
    if ! grep -qE "^\s*$i\s*\|" output.log; then
        echo "ERROR: Balance history missing data for process $i"
        exit 1
    fi
done

# Check for Total row
if ! grep -qE "^\s*Total\s*\|" output.log; then
    echo "ERROR: Balance history missing Total row"
    exit 1
fi

echo "  ✓ Balance history table validated"

echo ""
echo "All assertions passed!"