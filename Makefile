# QBF Solver - Makefile
#
# Build targets:
#   make          - Build the QBF solver
#   make debug    - Build with debug symbols
#   make clean    - Remove compiled files
#   make test     - Run solver on test cases
#   make generator - Build the random formula generator

CXX = g++
CC = gcc
CXXFLAGS = -Wall -Wextra -std=c++17 -O2
CXXFLAGS_DEBUG = -Wall -Wextra -std=c++17 -g3 -DDEBUG
CFLAGS = -Wall -Wextra -std=c99 -pedantic

# Main solver
SOLVER = qbf
SOLVER_SRC = main.cpp QBFPreprocessor.cpp QBFSolver.cpp

# Random formula generator
GENERATOR = blocksqbf
GENERATOR_SRC = blocksqbf.c

# Default target: build the solver
all: $(SOLVER)

$(SOLVER): $(SOLVER_SRC) QBFPreprocessor.h QBFSolver.h
	$(CXX) $(CXXFLAGS) -o $(SOLVER) $(SOLVER_SRC)

debug: $(SOLVER_SRC) QBFPreprocessor.h QBFSolver.h
	$(CXX) $(CXXFLAGS_DEBUG) -o $(SOLVER) $(SOLVER_SRC)

# Build the random formula generator (optional tool)
generator: $(GENERATOR_SRC)
	$(CC) $(CFLAGS) -O3 -o $(GENERATOR) $(GENERATOR_SRC)

# Run all tests
test: $(SOLVER)
	@echo "=== Running QBF Solver Tests ==="
	@echo ""
	@echo "1. Trivial SAT (expected: SATISFIABLE)"
	@./$(SOLVER) test/trivial_sat.qdimacs && echo "   PASS" || echo "   FAIL"
	@echo ""
	@echo "2. Trivial UNSAT (expected: UNSATISFIABLE)"
	@./$(SOLVER) test/trivial_unsat.qdimacs && echo "   FAIL (should be UNSAT)" || echo "   PASS"
	@echo ""
	@echo "3. FORALL both branches (expected: SATISFIABLE)"
	@./$(SOLVER) test/forall_both_branches.qdimacs && echo "   PASS" || echo "   FAIL"
	@echo ""
	@echo "4. FORALL wins (expected: UNSATISFIABLE)"
	@./$(SOLVER) test/forall_wins.qdimacs && echo "   FAIL (should be UNSAT)" || echo "   PASS"
	@echo ""
	@echo "5. EXISTS one branch (expected: SATISFIABLE)"
	@./$(SOLVER) test/exists_one_branch.qdimacs && echo "   PASS" || echo "   FAIL"
	@echo ""
	@echo "6. Unit propagation (expected: SATISFIABLE)"
	@./$(SOLVER) test/unit_propagation.qdimacs && echo "   PASS" || echo "   FAIL"
	@echo ""
	@echo "7. Alternating quantifiers (expected: SATISFIABLE)"
	@./$(SOLVER) test/alternating_quantifiers.qdimacs && echo "   PASS" || echo "   FAIL"
	@echo ""
	@echo "=== All tests completed ==="

clean:
	rm -f $(SOLVER) $(GENERATOR) *.o *~

.PHONY: all debug generator test clean
