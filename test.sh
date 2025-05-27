#!/bin/bash

echo "Building cache simulator..."
make clean
make

if [ ! -f "cacheSim" ]; then
    echo "Build failed! cacheSim executable not found."
    exit 1
fi

echo "Running tests..."
echo "The test succeeds if there are no diffs printed."
echo

# Count tests
total_tests=0
passed_tests=0

# Run all command files and generate output
for filename in tests/test*.command; do
    if [ -f "$filename" ]; then
        test_num=$(echo $filename | cut -d'.' -f1)
        echo "Running $test_num..."
        
        # Execute the command and capture output
        if bash ${filename} > ${test_num}.YoursOut 2>&1; then
            echo "  Command executed successfully"
        else
            echo "  Command execution failed"
        fi
    fi
done

# Compare outputs
for filename in tests/test*.out; do
    if [ -f "$filename" ]; then
        test_num=$(echo $filename | cut -d'.' -f1)
        total_tests=$((total_tests + 1))
        
        if [ -f "${test_num}.YoursOut" ]; then
            diff_result=$(diff ${test_num}.out ${test_num}.YoursOut)
            if [ "$diff_result" = "" ]; then
                echo "✓ Test ${test_num} PASSED"
                passed_tests=$((passed_tests + 1))
            else
                echo "✗ Test ${test_num} FAILED"
                echo "  Expected: $(cat ${test_num}.out)"
                echo "  Got:      $(cat ${test_num}.YoursOut)"
                echo "  Diff:"
                echo "$diff_result" | head -5
            fi
        else
            echo "✗ Test ${test_num} FAILED - No output file generated"
        fi
    fi
done

echo
echo "==============================================="
echo "Test Summary: $passed_tests/$total_tests tests passed"
if [ $passed_tests -eq $total_tests ]; then
    echo "🎉 All tests PASSED!"
else
    echo "❌ Some tests FAILED!"
fi
