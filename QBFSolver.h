/*
 * QBFSolver.h - DPLL-based Quantified Boolean Formula Solver
 *
 * This solver implements a recursive DPLL algorithm adapted for QBF.
 *
 * KEY CONCEPT - QBF vs SAT:
 * In SAT, we just need to find ONE satisfying assignment.
 * In QBF, we must handle TWO types of variables:
 *
 *   EXISTENTIAL (∃): We need to find SOME value that works
 *                    → Formula is SAT if EITHER true OR false branch succeeds
 *
 *   UNIVERSAL (∀):   The formula must work for ALL values
 *                    → Formula is SAT only if BOTH true AND false branches succeed
 *
 * GAME INTERPRETATION:
 * Think of it as a two-player game:
 *   - EXISTS player tries to SATISFY the formula
 *   - FORALL player tries to FALSIFY the formula
 * The formula is true iff EXISTS has a winning strategy.
 */

#ifndef QBF_SOLVER_H
#define QBF_SOLVER_H

#include "QBFPreprocessor.h"
#include <unordered_map>
#include <string>

// Result of solving: SAT (true), UNSAT (false), or still working
enum class Result { SAT, UNSAT };

class QBFSolver {
private:
    // Formula state (copied from preprocessor, modified during search)
    std::vector<QuantifierBlock> quantifierBlocks;
    std::vector<Clause> clauses;
    std::unordered_map<int, bool> assignments;

    // Maps for quick lookup
    std::unordered_map<int, Quantifier> varToQuantifier;
    std::unordered_map<int, int> varToBlockIndex;

    // Verbose mode for educational tracing
    bool verbose;
    int depth;  // Current recursion depth (for indentation)

    // Core solving methods
    Result solve_recursive();

    // Helper methods
    int findNextUnassignedVar() const;
    void assignVariable(int var, bool value);
    void unassignVariable(int var);
    bool hasEmptyClause() const;
    bool allClausesSatisfied() const;
    void simplifyWithAssignment(int var, bool value);
    void restoreClauses(const std::vector<Clause>& saved);

    // Verbose output helpers
    void log(const std::string& msg) const;
    std::string indent() const;

public:
    QBFSolver();

    // Main entry point - solves the QBF formula
    Result solve(const QBFPreprocessor& preprocessor);

    // Enable verbose mode for step-by-step tracing
    void setVerbose(bool v);

    // Get final assignments (for SAT results)
    const std::unordered_map<int, bool>& getAssignments() const;
};

#endif // QBF_SOLVER_H
