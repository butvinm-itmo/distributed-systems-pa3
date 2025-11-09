#!/bin/bash
# Run all bank robbery scenario tests

echo "=========================================="
echo "Running All Bank Robbery Scenario Tests"
echo "=========================================="
echo ""

# Make all test scripts executable
chmod +x test_scenario0.sh test_scenario1.sh test_scenario2.sh test_scenario3.sh

# Track results
PASSED=0
FAILED=0
TOTAL=4

# Run each scenario test
for scenario in 0 1 2 3; do
    echo ""
    echo "================================================"
    echo "Running Scenario $scenario..."
    echo "================================================"

    if ./test_scenario${scenario}.sh; then
        PASSED=$((PASSED + 1))
        echo "✓ Scenario $scenario PASSED"
    else
        FAILED=$((FAILED + 1))
        echo "✗ Scenario $scenario FAILED"
    fi
done

echo ""
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo "Total scenarios: $TOTAL"
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "🎉 All scenarios passed!"
    exit 0
else
    echo "⚠️  Some scenarios failed"
    exit 1
fi
