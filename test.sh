#!/bin/bash

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_status() {
    echo -e "${GREEN}[TEST]${NC} $1"
}

print_error() {
    echo -e "${RED}[TEST]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[TEST]${NC} $1"
}

EXPECTED_USAGE="Usage: .* <queue_size> <plugin1> <plugin2> ... <pluginN>
Arguments:
  queue_size    Maximum number of items in each plugin's queue
  plugin1..N    Names of plugins to load (without .so extension)
Available plugins:
  logger        - Logs all strings that pass through
  typewriter    - Simulates typewriter effect with delays
  uppercaser    - Converts strings to uppercase
  rotator       - Move every character to the right. Last character moves to the beginning.
  flipper       - Reverses the order of characters
  expander      - Expands each character with spaces
Example:
  .* 20 uppercaser rotator logger"

# Build the project first
print_status "Building project..."
./build.sh

cd output

test_count=0
pass_count=0
failed_tests=()

print_status "Starting comprehensive integration tests..."

run_test() {
    local test_name="$1"
    local input="$2"
    local command="$3"
    local expected_pattern="$4"
    local expect_error="$5"
    local check_usage="$6"
    
    test_count=$((test_count + 1))
    print_status "Running test $test_count: $test_name"
    
    # Run the test and capture output
    local actual_output
    actual_output=$(echo -e "$input" | eval "$command" 2>&1 || true)
    local exit_code=$?
    
    local test_passed=true
    local failure_reason=""
    
    # Check exit code expectations
    if [ "$expect_error" = "expect_error" ]; then
        if [ $exit_code -eq 0 ]; then
            test_passed=false
            failure_reason="Expected non-zero exit code but got 0"
        fi
    else
        if [ $exit_code -ne 0 ]; then
            test_passed=false
            failure_reason="Expected zero exit code but got $exit_code"
        fi
    fi
    
    # Check if pattern matches - support multiline patterns
    if [ "$test_passed" = true ]; then
        local pattern_found=true
        local missing_patterns=""
        
        while IFS= read -r single_pattern; do
            if [ -n "$single_pattern" ]; then
                if ! echo "$actual_output" | grep -q "$single_pattern"; then
                    pattern_found=false
                    missing_patterns="$missing_patterns'$single_pattern' "
                fi
            fi
        done << EOF
$expected_pattern
EOF
        
        if [ "$pattern_found" = false ]; then
            test_passed=false
            failure_reason="Expected patterns not found in output: $missing_patterns"
        fi
    fi
    
    # Check usage output if required
    if [ "$test_passed" = true ] && [ "$check_usage" = "EXPECTED_USAGE" ]; then
        if ! echo "$actual_output" | grep -qE "$EXPECTED_USAGE"; then
            test_passed=false
            failure_reason="Usage message not found in output"
        fi
    fi
    
    if [ "$test_passed" = true ]; then
        print_status "PASS: $test_name"
        pass_count=$((pass_count + 1))
    else
        print_error "FAIL: $test_name"
        print_error "Reason: $failure_reason"
        print_error "Expected pattern: $expected_pattern"
        print_error "Actual output:"
        echo "$actual_output" | sed 's/^/  /'
        echo
        failed_tests+=("$test_name")
    fi
}

# SECTION 1: ARGUMENT VALIDATION TESTS
print_status "ARGUMENT VALIDATION TESTS"

run_test "No arguments provided" \
    "" \
    "./analyzer" \
    "Error: Invalid number of arguments" \
    "check_usage" \
    "expect_error"

run_test "Only queue size provided" \
    "" \
    "./analyzer 10" \
    "Error: Invalid number of arguments" \
    "check_usage" \
    "expect_error"

run_test "Zero queue size" \
    "" \
    "./analyzer 0 logger" \
    "Error: Invalid queue size" \
    "check_usage" \
    "expect_error"

run_test "Negative queue size" \
    "" \
    "./analyzer -5 logger" \
    "Error: Invalid queue size" \
    "check_usage" \
    "expect_error"

run_test "Non-numeric queue size" \
    "" \
    "./analyzer abc logger" \
    "Error: Invalid queue size" \
    "check_usage" \
    "expect_error"

run_test "Non-existent plugin" \
    "" \
    "./analyzer 10 nonexistent" \
    "Error loading plugin nonexistent" \
    "check_usage" \
    "expect_error"

run_test "Invalid plugin in middle of valid ones" \
    "" \
    "./analyzer 10 logger invalid flipper" \
    "Error loading plugin invalid" \
    "check_usage" \
    "expect_error"

run_test "Plugin name with extension" \
    "" \
    "./analyzer 10 logger.so" \
    "Error loading plugin logger.so" \
    "check_usage" \
    "expect_error"

# SECTION 2: BASIC PLUGIN FUNCTIONALITY TESTS
print_status "BASIC PLUGIN FUNCTIONALITY TESTS"

run_test "Basic logger test" \
    "hello world\n<END>" \
    "./analyzer 10 logger" \
    "\\[logger\\] hello world" \
    "" \
    ""

run_test "Uppercaser functionality" \
    "Hello World123\n<END>" \
    "./analyzer 10 uppercaser logger" \
    "\\[logger\\] HELLO WORLD123" \
    "" \
    ""

run_test "Rotator functionality" \
    "abcdef\n<END>" \
    "./analyzer 10 rotator logger" \
    "\\[logger\\] fabcde" \
    "" \
    ""

run_test "Flipper functionality" \
    "hello\n<END>" \
    "./analyzer 10 flipper logger" \
    "\\[logger\\] olleh" \
    "" \
    ""

run_test "Expander functionality" \
    "abc\n<END>" \
    "./analyzer 10 expander logger" \
    "\\[logger\\] a b c" \
    "" \
    ""

run_test "Typewriter functionality" \
    "hi\n<END>" \
    "timeout 10 ./analyzer 10 typewriter" \
    "\\[typewriter\\] hi" \
    "" \
    ""

# SECTION 3: PIPELINE CHAIN TESTS
print_status "PIPELINE CHAIN TESTS"

run_test "Two-plugin chain" \
    "hello\n<END>" \
    "./analyzer 10 uppercaser logger" \
    "\\[logger\\] HELLO" \
    "" \
    ""

run_test "Multiple inputs logger->typewriter" \
    "hello\nworld\nomer\n<END>" \
    "timeout 15 ./analyzer 10 logger typewriter" \
    "\\[logger\\] hello
\\[logger\\] world
\\[logger\\] omer
\\[typewriter\\] hello
\\[typewriter\\] world
\\[typewriter\\] omer" \
    "" \
    ""

run_test "Three-plugin chain" \
    "abcd\n<END>" \
    "./analyzer 10 uppercaser rotator logger" \
    "\\[logger\\] DABC" \
    "" \
    ""

run_test "Four-plugin chain" \
    "test\n<END>" \
    "./analyzer 15 uppercaser rotator flipper logger" \
    "\\[logger\\] SETT" \
    "" \
    ""

run_test "Five-plugin chain" \
    "hello\n<END>" \
    "./analyzer 15 uppercaser rotator flipper expander logger" \
    "\\[logger\\] L L E H O" \
    "" \
    ""

# SECTION 4: NEW DIVERSE TRANSFORMATION TESTS
print_status "DIVERSE TRANSFORMATION TESTS"

run_test "Complex Hebrew text (ASCII)" \
    "shalom\n<END>" \
    "./analyzer 10 uppercaser rotator flipper expander logger" \
    "\\[logger\\] O L A H S M" \
    "" \
    ""

run_test "Mixed case and numbers" \
    "Test123\n<END>" \
    "./analyzer 10 uppercaser flipper logger" \
    "\\[logger\\] 321TSET" \
    "" \
    ""

run_test "Single character transformations" \
    "x\n<END>" \
    "./analyzer 10 uppercaser rotator flipper expander logger" \
    "\\[logger\\] X" \
    "" \
    ""

run_test "Palindrome test" \
    "racecar\n<END>" \
    "./analyzer 10 flipper logger" \
    "\\[logger\\] racecar" \
    "" \
    ""

run_test "All same character" \
    "aaaa\n<END>" \
    "./analyzer 10 rotator expander logger" \
    "\\[logger\\] a a a a" \
    "" \
    ""

run_test "Numbers only rotation" \
    "12345\n<END>" \
    "./analyzer 10 rotator logger" \
    "\\[logger\\] 51234" \
    "" \
    ""

run_test "Special chars with expander" \
    "^@#\n<END>" \
    "./analyzer 10 expander logger" \
    "\\[logger\\] ^ @ #" \
    "" \
    ""

# SECTION 5: PLUGIN COMBINATION STRESS TESTS  
print_status "PLUGIN COMBINATION STRESS TESTS"

run_test "Double rotation" \
    "abcde\n<END>" \
    "./analyzer 10 rotator logger" \
    "\\[logger\\] eabcd" \
    "" \
    ""

run_test "Double flipper (should return original)" \
    "testing\n<END>" \
    "./analyzer 10 flipper logger" \
    "\\[logger\\] gnitset" \
    "" \
    ""

run_test "Uppercase then lowercase simulation" \
    "MixedCase\n<END>" \
    "./analyzer 10 uppercaser logger" \
    "\\[logger\\] MIXEDCASE" \
    "" \
    ""

run_test "Expander then rotator" \
    "abc\n<END>" \
    "./analyzer 15 expander rotator logger" \
    "\\[logger\\] ca b " \
    "" \
    ""

# SECTION 6: QUEUE SIZE EDGE CASES
print_status "QUEUE SIZE EDGE CASES"

run_test "Minimal queue size (1)" \
    "test\n<END>" \
    "./analyzer 1 uppercaser logger" \
    "\\[logger\\] TEST" \
    "" \
    ""

run_test "Large queue with single item" \
    "single\n<END>" \
    "./analyzer 1000 uppercaser logger" \
    "\\[logger\\] SINGLE" \
    "" \
    ""

# SECTION 7: MULTIPLE INPUT VARIATIONS
print_status "MULTIPLE INPUT VARIATIONS"

run_test "Sequential processing verification" \
    "firstsecondthird\n<END>" \
    "./analyzer 10 uppercaser logger" \
    "\\[logger\\] FIRSTSECONDTHIRD" \
    "" \
    ""

run_test "Empty lines mixed with content" \
    "test\n\ntest2\n<END>" \
    "./analyzer 10 uppercaser logger" \
    "\\[logger\\] TEST" \
    "" \
    ""

# SECTION 8: EDGE CASE STRESS TESTS
print_status "EDGE CASE STRESS TESTS"

run_test "Empty string processing" \
    "\n<END>" \
    "./analyzer 10 expander logger" \
    "\\[logger\\]" \
    "" \
    ""

# SECTION 9: REALISTIC USAGE SCENARIOS
print_status "REALISTIC USAGE SCENARIOS"

run_test "Text formatting pipeline" \
    "format me\n<END>" \
    "./analyzer 20 uppercaser expander logger" \
    "\\[logger\\] F O R M A T   M E" \
    "" \
    ""

run_test "Reverse engineering check" \
    "decode\n<END>" \
    "./analyzer 15 flipper uppercaser rotator logger" \
    "\\[logger\\] DEDOCE" \
    "" \
    ""

# SECTION 10: MEMORY AND PERFORMANCE TESTS
print_status "MEMORY AND PERFORMANCE TESTS"

run_test "Rapid fire small strings" \
    "$(for i in {1..20}; do echo -n "s$i\n"; done)<END>" \
    "./analyzer 3 logger" \
    "\\[logger\\] s1" \
    "" \
    "" \
    $(for i in {2..20}; do echo "\\[logger\\] s$i"; done)

run_test "Large string with all transformations" \
    "$(printf 'BigString%.0s' {1..10})\n<END>" \
    "./analyzer 30 uppercaser rotator flipper expander logger" \
    "\\[logger\\]" \
    "" \
    ""

run_test "High throughput test" \
    "$(for i in {1..50}; do echo -n "$i\n"; done)<END>" \
    "./analyzer 10 uppercaser logger" \
    "\\[logger\\] 1" \
    "" \
    "" \
    $(for i in {2..50}; do echo "\\[logger\\] $i"; done)

# SECTION 11: PLUGIN ORDER VARIATIONS
print_status "PLUGIN ORDER VARIATIONS"

run_test "Reverse plugin order effect" \
    "test\n<END>" \
    "./analyzer 10 logger uppercaser" \
    "\\[logger\\] test" \
    "" \
    ""

run_test "Expander first vs last" \
    "abc\n<END>" \
    "./analyzer 15 uppercaser expander flipper logger" \
    "\\[logger\\] C B A" \
    "" \
    ""

# SECTION 12: BOUNDARY AND STRESS CONDITIONS
print_status "BOUNDARY AND STRESS CONDITIONS"

run_test "Just END token" \
    "<END>" \
    "./analyzer 10 logger" \
    "Pipeline shutdown complete" \
    "" \
    ""

run_test "Minimal viable input" \
    "a\n<END>" \
    "./analyzer 1 logger" \
    "\\[logger\\] a" \
    "" \
    ""

# SECTION 13: UNICODE AND SPECIAL ENCODING (ASCII only)
print_status "SPECIAL CHARACTER TESTS"

run_test "All printable ASCII" \
    "ABC123%@#\n<END>" \
    "./analyzer 10 flipper logger" \
    "\\[logger\\] #@%321CBA" \
    "" \
    ""

run_test "Numeric strings only" \
    "987654321\n<END>" \
    "./analyzer 10 rotator flipper logger" \
    "\\[logger\\] 234567891" \
    "" \
    ""

# SECTION 14: PERFORMANCE WITH TYPEWRITER
print_status "TYPEWRITER INTEGRATION TESTS"

run_test "Typewriter with simple input" \
    "hi\n<END>" \
    "timeout 15 ./analyzer 10 typewriter" \
    "\\[typewriter\\] hi" \
    "" \
    ""

run_test "Logger before typewriter" \
    "test\n<END>" \
    "timeout 15 ./analyzer 10 logger typewriter" \
    "\\[logger\\] test" \
    "" \
    ""

run_test "Transformation before typewriter" \
    "abc\n<END>" \
    "timeout 20 ./analyzer 15 uppercaser expander typewriter" \
    "\\[typewriter\\] A B C" \
    "" \
    ""

# SECTION 15: CREATIVE AND CHALLENGING TESTS
print_status "CREATIVE AND CHALLENGING TESTS"

run_test "Fibonacci sequence start" \
    "1123\n<END>" \
    "./analyzer 10 rotator flipper uppercaser logger" \
    "\\[logger\\] 2113" \
    "" \
    ""

run_test "Symmetric string test" \
    "abba\n<END>" \
    "./analyzer 10 flipper logger" \
    "\\[logger\\] abba" \
    "" \
    ""

run_test "All vowels transformation" \
    "aeiou\n<END>" \
    "./analyzer 15 uppercaser rotator expander logger" \
    "\\[logger\\] U A E I O" \
    "" \
    ""

run_test "Binary-like string" \
    "101010\n<END>" \
    "./analyzer 10 rotator flipper logger" \
    "\\[logger\\] 101010" \
    "" \
    ""

run_test "Alternating case simulation" \
    "AbCdEf\n<END>" \
    "./analyzer 10 uppercaser flipper logger" \
    "\\[logger\\] FEDCBA" \
    "" \
    ""

run_test "Mathematical expression" \
    "2+2=4\n<END>" \
    "./analyzer 10 flipper expander logger" \
    "\\[logger\\] 4 = 2 + 2" \
    "" \
    ""

run_test "DNA sequence simulation" \
    "ATCG\n<END>" \
    "./analyzer 10 rotator flipper expander logger" \
    "\\[logger\\] C T A G" \
    "" \
    ""

run_test "Repeated pattern" \
    "abcabc\n<END>" \
    "./analyzer 15 rotator expander logger" \
    "\\[logger\\] c a b c a b" \
    "" \
    ""

run_test "Count sequence" \
    "12321\n<END>" \
    "./analyzer 10 flipper rotator logger" \
    "\\[logger\\] 11232" \
    "" \
    ""

# SECTION 16: EXTREME EDGE CASES
print_status "EXTREME EDGE CASES"

run_test "Single repeated character" \
    "aaaaa\n<END>" \
    "./analyzer 10 rotator flipper expander logger" \
    "\\[logger\\] a a a a a" \
    "" \
    ""

run_test "Ascending numbers" \
    "123456789\n<END>" \
    "./analyzer 15 rotator logger" \
    "\\[logger\\] 912345678" \
    "" \
    ""

run_test "Descending letters" \
    "zyxwvu\n<END>" \
    "./analyzer 10 uppercaser flipper logger" \
    "\\[logger\\] UVWXYZ" \
    "" \
    ""

run_test "Mixed symbols and letters" \
    "a1b2c3\n<END>" \
    "./analyzer 15 expander rotator logger" \
    "\\[logger\\] 3a 1 b 2 c " \
    "" \
    ""

run_test "Whitespace handling" \
    "a b c\n<END>" \
    "./analyzer 10 flipper logger" \
    "\\[logger\\] c b a" \
    "" \
    ""

# SECTION 17: PLUGIN INTERACTION STRESS
print_status "PLUGIN INTERACTION STRESS TESTS"

run_test "Alternating transformations" \
    "abcdef\n<END>" \
    "./analyzer 20 flipper rotator logger" \
    "\\[logger\\] afedcb" \
    "" \
    ""

run_test "Expansion stress test" \
    "12345\n<END>" \
    "./analyzer 25 expander logger" \
    "\\[logger\\] 1 2 3 4 5" \
    "" \
    ""

# SECTION 18: REAL-WORLD SCENARIOS
print_status "REAL-WORLD SCENARIO TESTS"

run_test "Data cleaning simulation" \
    "CleanMe123\n<END>" \
    "./analyzer 20 uppercaser expander logger" \
    "\\[logger\\] C L E A N M E 1 2 3" \
    "" \
    ""

run_test "Text processing workflow" \
    "workflow\n<END>" \
    "./analyzer 25 uppercaser flipper logger" \
    "\\[logger\\] WOLFKROW" \
    "" \
    ""

run_test "Encryption simulation" \
    "secret\n<END>" \
    "./analyzer 20 flipper uppercaser rotator logger" \
    "\\[logger\\] STERCE" \
    "" \
    ""

run_test "Validation chain" \
    "validate123\n<END>" \
    "./analyzer 30 uppercaser rotator flipper expander logger" \
    "\\[logger\\] 2 1 E T A D I L A V 3" \
    "" \
    ""

# SECTION 19: PERFORMANCE BENCHMARKS
print_status "PERFORMANCE BENCHMARK TESTS"

run_test "Rapid sequential processing" \
    "$(for i in {1..10}; do echo -n "item$i\n"; done)<END>" \
    "./analyzer 5 uppercaser logger" \
    "\\[logger\\] ITEM1" \
    "" \
    ""

run_test "Large buffer test" \
    "$(printf 'BufferTest%.0s' {1..5})\n<END>" \
    "./analyzer 50 rotator logger" \
    "\\[logger\\] tBufferTestBufferTestBufferTestBufferTes" \
    "" \
    ""

run_test "Memory stress with transformations" \
    "$(printf 'X%.0s' {1..100})\n<END>" \
    "./analyzer 25 flipper expander logger" \
    "\\[logger\\]" \
    "" \
    ""

# SECTION 20: ERROR BOUNDARY TESTING  
print_status "ERROR BOUNDARY TESTS"

run_test "Queue overflow simulation" \
    "$(for i in {1..20}; do echo -n "$i\n"; done)<END>" \
    "./analyzer 2 uppercaser logger" \
    "\\[logger\\] 1" \
    "" \
    ""

run_test "Rapid input with delay" \
    "fast1\nfast2\nfast3\n<END>" \
    "timeout 20 ./analyzer 5 logger typewriter" \
    "\\[logger\\] fast1" \
    "" \
    ""
print_status "COMPREHENSIVE INTEGRATION TESTS"

run_test "All plugins except typewriter" \
    "integration\n<END>" \
    "./analyzer 30 uppercaser rotator flipper expander logger" \
    "\\[logger\\] O I T A R G E T N I N" \
    "" \
    ""

run_test "Real world scenario simulation" \
    "ProcessThis\n<END>" \
    "./analyzer 25 uppercaser rotator flipper logger" \
    "\\[logger\\] IHTSSECORPS" \
    "" \
    ""

run_test "Data validation chain" \
    "ValidateMe\n<END>" \
    "./analyzer 20 uppercaser rotator logger" \
    "\\[logger\\] EVALIDATEM" \
    "" \
    ""

# FINAL RESULTS
print_status "TEST EXECUTION COMPLETE"
print_status "Total tests executed: $test_count"
print_status "Tests passed: $pass_count"
print_status "Tests failed: $((test_count - pass_count))"

if [ $pass_count -eq $test_count ]; then
    print_status "CONGRATULATIONS!"
    print_status "All tests passed successfully!"
    print_status "System is ready for production use!"
    exit 0
else
    print_error "TESTS FAILED"
    print_error "The following tests did not pass:"
    for failed_test in "${failed_tests[@]}"; do
        print_error "$failed_test"
    done
    exit 1
fi