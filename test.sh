#!/bin/bash

# Usage function
show_usage() {
    echo "Usage: $0 [test_count] [show_details] [parallel_jobs]"
    echo "  test_count: 'all' or number of tests per directory (default: all)"
    echo "  show_details: 'true' or 'false' for verbose output (default: false)"
    echo "  parallel_jobs: number of concurrent test processes (default: 16)"
    echo "Examples:"
    echo "  $0                    # Run all tests with 16 parallel jobs"
    echo "  $0 50                 # Run first 50 tests per directory"
    echo "  $0 all true           # Run all tests with detailed output"
    echo "  $0 all false 16       # Run all tests with 16 parallel jobs"
    exit 1
}

# Check for help flag
if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    show_usage
fi

# Check if user wants to run a subset of tests
SUBSET_SIZE=${1:-"all"}
SHOW_DETAILS=${2:-"false"}
MAX_PARALLEL_JOBS=${3:-16}  # Default to 16 parallel jobs

# Validate parallel jobs parameter
if ! [[ "$MAX_PARALLEL_JOBS" =~ ^[0-9]+$ ]] || [ "$MAX_PARALLEL_JOBS" -lt 1 ]; then
    echo "Error: parallel_jobs must be a positive integer"
    show_usage
fi

echo "Building cache simulator..."
make clean
make

if [ ! -f "cacheSim" ]; then
    echo "Build failed! cacheSim executable not found."
    exit 1
fi

echo "Running tests in parallel (max $MAX_PARALLEL_JOBS concurrent jobs)..."
if [ "$SUBSET_SIZE" != "all" ]; then
    echo "Running first $SUBSET_SIZE tests from each directory..."
else
    echo "Running ALL tests (this may take a while)..."
fi
echo "The test succeeds if there are no diffs printed."
echo

# Count tests
total_tests=0
passed_tests=0
failed_tests=0
failed_details=()
failed_test_count=0  # Separate counter for failed test details
max_failed_details=20

# Array to track background job PIDs
declare -a job_pids=()
declare -a test_commands=()
declare -a test_files=()

# Function to run a single test in background
run_single_test() {
    local test_file=$1
    local test_dir=$2
    
    test_num=$(echo $test_file | cut -d'.' -f1)
    cmd_file="$test_dir/$(basename $test_num).command"
    
    if [ -f "$cmd_file" ]; then
        # Read the command and fix the path if necessary
        command=$(cat "$cmd_file")
        if [ "$test_dir" = "test_t" ]; then
            command=$(echo "$command" | sed 's|tests/|test_t/|g')
        fi
        
        # Execute the command and capture output
        eval "$command" > ${test_num}.YoursOut 2>&1
    fi
}

# Function to wait for jobs and control parallel execution
wait_for_slot() {
    while [ ${#job_pids[@]} -ge $MAX_PARALLEL_JOBS ]; do
        # Check which jobs have finished
        new_pids=()
        for pid in "${job_pids[@]}"; do
            if kill -0 "$pid" 2>/dev/null; then
                new_pids+=("$pid")
            fi
        done
        job_pids=("${new_pids[@]}")
        
        if [ ${#job_pids[@]} -ge $MAX_PARALLEL_JOBS ]; then
            sleep 0.1  # Short sleep to prevent busy waiting
        fi
    done
}

# Function to run tests in a directory with parallelization
run_tests_in_dir() {
    local test_dir=$1
    local max_tests=$2
    local current_count=0
    local dir_total_tests=0
    
    # Count total tests in this directory first
    for filename in $test_dir/test*.out; do
        if [ -f "$filename" ]; then
            dir_total_tests=$((dir_total_tests + 1))
        fi
    done
    
    # Limit to max_tests if specified
    if [ "$max_tests" -lt "$dir_total_tests" ]; then
        dir_total_tests=$max_tests
    fi
    
    echo ""
    echo "============================================================"
    echo "  Processing $dir_total_tests tests in $test_dir/ directory..."
    echo "============================================================"
    
    # Launch test commands in parallel
    for filename in $test_dir/test*.command; do
        if [ -f "$filename" ] && [ "$current_count" -lt "$max_tests" ]; then
            current_count=$((current_count + 1))
            
            if [ "$SHOW_DETAILS" = "true" ]; then
                echo "Launching $filename... ($current_count/$dir_total_tests)"
            else
                # Show progress every 50 tests
                if [ $((current_count % 50)) -eq 0 ]; then
                    echo "  Progress: $current_count/$dir_total_tests tests launched"
                fi
            fi
            
            # Wait for an available slot
            wait_for_slot
            
            # Launch test in background
            run_single_test "$filename" "$test_dir" &
            job_pids+=($!)
        fi
    done
    
    # Wait for all remaining jobs to complete
    echo "  Waiting for all tests to complete..."
    for pid in "${job_pids[@]}"; do
        wait "$pid" 2>/dev/null
    done
    job_pids=()  # Clear the array for next directory
    
    echo "Comparing results in $test_dir/ directory..."
    current_count=0
    local dir_passed=0
    local dir_failed=0
    
    # Compare outputs
    for filename in $test_dir/test*.out; do
        if [ -f "$filename" ] && [ "$current_count" -lt "$max_tests" ]; then
            test_num=$(echo $filename | cut -d'.' -f1)
            total_tests=$((total_tests + 1))
            current_count=$((current_count + 1))
            
            if [ -f "${test_num}.YoursOut" ]; then
                diff_result=$(diff ${test_num}.out ${test_num}.YoursOut)
                if [ "$diff_result" = "" ]; then
                    if [ "$SHOW_DETAILS" = "true" ]; then
                        echo "✓ Test ${test_num} PASSED"
                    fi
                    passed_tests=$((passed_tests + 1))
                    dir_passed=$((dir_passed + 1))
                else
                    echo "✗ Test ${test_num} FAILED"
                    failed_tests=$((failed_tests + 1))
                    dir_failed=$((dir_failed + 1))
                    
                    # Store details for first 20 failed tests
                    if [ $failed_test_count -lt $max_failed_details ]; then
                        failed_test_count=$((failed_test_count + 1))
                        
                        # Get the command that was executed
                        cmd_file="$test_dir/$(basename $test_num).command"
                        if [ -f "$cmd_file" ]; then
                            executed_command=$(cat "$cmd_file")
                            if [ "$test_dir" = "test_t" ]; then
                                executed_command=$(echo "$executed_command" | sed 's|tests/|test_t/|g')
                            fi
                        else
                            executed_command="Command file not found"
                        fi
                        
                        expected_output=$(cat ${test_num}.out 2>/dev/null || echo "Expected output file not found")
                        actual_output=$(cat ${test_num}.YoursOut 2>/dev/null || echo "Actual output file not found")
                        
                        # Store each detail as a separate array element
                        failed_details+=("========== FAILED TEST ${test_num} ==========")
                        failed_details+=("Command: $executed_command")
                        failed_details+=("Expected: $expected_output")
                        failed_details+=("Got:      $actual_output")
                        failed_details+=("Diff:")
                        
                        # Handle diff output more carefully - store each line separately
                        while IFS= read -r line; do
                            failed_details+=("$line")
                        done <<< "$(echo "$diff_result" | head -5)"
                        
                        failed_details+=("")  # Empty line separator
                    fi
                    
                    if [ "$SHOW_DETAILS" = "true" ]; then
                        echo "  Expected: $(cat ${test_num}.out)"
                        echo "  Got:      $(cat ${test_num}.YoursOut)"
                        echo "  Diff:"
                        echo "$diff_result" | head -3
                    fi
                fi
            else
                echo "✗ Test ${test_num} FAILED - No output file generated"
                failed_tests=$((failed_tests + 1))
                dir_failed=$((dir_failed + 1))
                
                # Store details for missing output files too
                if [ $failed_test_count -lt $max_failed_details ]; then
                    failed_test_count=$((failed_test_count + 1))
                    cmd_file="$test_dir/$(basename $test_num).command"
                    if [ -f "$cmd_file" ]; then
                        executed_command=$(cat "$cmd_file")
                        if [ "$test_dir" = "test_t" ]; then
                            executed_command=$(echo "$executed_command" | sed 's|tests/|test_t/|g')
                        fi
                    else
                        executed_command="Command file not found"
                    fi
                    
                    failed_details+=("========== FAILED TEST ${test_num} ==========")
                    failed_details+=("Command: $executed_command")
                    failed_details+=("Error: No output file generated")
                    failed_details+=("")
                fi
            fi
        fi
    done
    
    echo "Results for $test_dir/: $dir_passed passed, $dir_failed failed out of $((dir_passed + dir_failed)) tests"
}

# Determine how many tests to run per directory
if [ "$SUBSET_SIZE" = "all" ]; then
    # Count actual total tests available
    tests_dir_count=$(ls tests/test*.out 2>/dev/null | wc -l)
    test_t_dir_count=$(ls test_t/test*.out 2>/dev/null | wc -l)
    total_available=$((tests_dir_count + test_t_dir_count))
    MAX_TESTS=999999  # Effectively unlimited
    echo "Found $total_available total tests ($tests_dir_count in tests/, $test_t_dir_count in test_t/)"
else
    MAX_TESTS=$SUBSET_SIZE
    echo "Will run first $MAX_TESTS tests from each directory"
fi

# Run tests in both directories
run_tests_in_dir "tests" $MAX_TESTS
# run_tests_in_dir "test_t" $MAX_TESTS

echo
echo "============================================================"
echo "                        TEST SUMMARY"
echo "============================================================"
echo "Total Tests: $total_tests"
echo "Passed: $passed_tests"
echo "Failed: $failed_tests"
echo "Success Rate: $(echo "scale=1; $passed_tests * 100 / $total_tests" | bc -l)%"

if [ $passed_tests -eq $total_tests ]; then
    echo "🎉 All tests PASSED!"
else
    echo "❌ Some tests FAILED!"
    
    if [ ${#failed_details[@]} -gt 0 ]; then
        echo ""
        echo "============================================================"
        echo "          DETAILS OF FIRST $failed_test_count FAILED TESTS"
        echo "============================================================"
        for detail in "${failed_details[@]}"; do
            echo "$detail"
        done
        
        if [ $failed_tests -gt $failed_test_count ]; then
            echo "... and $((failed_tests - failed_test_count)) more failed tests (details omitted)"
        fi
    fi
fi
