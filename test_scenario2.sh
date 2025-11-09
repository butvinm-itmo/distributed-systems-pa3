#!/bin/bash
# Test Scenario 2: Robin Hood - Redistribute Wealth
# Transfers: 3→1 $1, 2→1 $2
# Expected final balances: P1=$13, P2=$18, P3=$29 (Total=$60)

set -e

echo "=========================================="
echo "Testing Scenario 2: Robin Hood - Redistribute Wealth"
echo "=========================================="

# Build with scenario 2
cp bank_robbery_scenario2.c.bak bank_robbery.c
./build.sh

# Run the program
./run.sh -p 3 10 20 30 2>&1 | tee output.log

# Validate events.log
NUM_PROCESSES=3
TOTAL_PROCESSES=4

echo ""
echo "Validating events.log..."
for ((i=0; i<TOTAL_PROCESSES; i++)); do
    if ! grep -qE "^[0-9]+: process $i received all STARTED messages$" events.log; then
        echo "ERROR: Process $i did not receive all STARTED messages"
        exit 1
    fi
    if ! grep -qE "^[0-9]+: process $i received all DONE messages$" events.log; then
        echo "ERROR: Process $i did not receive all DONE messages"
        exit 1
    fi
done

# Validate specific transfers occurred
echo "Validating transfers..."
if ! grep -qE "process 3 transferred \\\$ 1 to process 1" events.log; then
    echo "ERROR: Transfer 3→1 \$1 not found"
    exit 1
fi
if ! grep -qE "process 2 transferred \\\$ 2 to process 1" events.log; then
    echo "ERROR: Transfer 2→1 \$2 not found"
    exit 1
fi

# Validate balance history table
echo "Validating balance history..."
if ! grep -q "Full balance history" output.log; then
    echo "ERROR: Balance history table not found"
    exit 1
fi

# Validate total balance is preserved
if ! grep -qE "Total.*60" output.log; then
    echo "ERROR: Total balance not preserved at \$60"
    exit 1
fi

echo ""
echo "✓ Scenario 2 tests passed!"
echo ""
