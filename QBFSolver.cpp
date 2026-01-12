/*
 * QBFSolver.cpp - Implementation of DPLL-based QBF Solver
 *
 * ALGORITHM OVERVIEW:
 * ===================
 *
 * The solver uses a recursive DPLL (Davis-Putnam-Logemann-Loveland) algorithm
 * adapted for quantified boolean formulas. The key insight is that QBF solving
 * is like playing a game:
 *
 *   - Variables are processed in quantifier prefix order (left to right)
 *   - For ∃x: EXISTS player chooses x's value, trying to satisfy the formula
 *   - For ∀x: FORALL player chooses x's value, trying to falsify the formula
 *
 * The formula is TRUE if EXISTS has a winning strategy (can satisfy formula
 * no matter what FORALL does).
 *
 * PSEUDO-CODE:
 * ============
 *
 *   solve(formula):
 *     if formula has empty clause: return UNSAT  (contradiction found)
 *     if all clauses satisfied: return SAT       (winning state)
 *
 *     x = next unassigned variable (following quantifier order)
 *
 *     if x is EXISTENTIAL:
 *       // EXISTS wins if it can find ANY satisfying value
 *       if solve(formula[x=true]) == SAT: return SAT
 *       if solve(formula[x=false]) == SAT: return SAT
 *       return UNSAT  // Neither value works
 *
 *     if x is UNIVERSAL:
 *       // FORALL wins (returns UNSAT) if it can find ANY falsifying value
 *       // So EXISTS needs BOTH branches to succeed
 *       if solve(formula[x=true]) == UNSAT: return UNSAT
 *       if solve(formula[x=false]) == UNSAT: return UNSAT
 *       return SAT  // Both values work
 *
 * VISUAL EXAMPLE:
 * ===============
 *
 *   Formula: ∀x ∃y (x ∨ y) ∧ (¬x ∨ ¬y)
 *
 *   Search tree:
 *
 *                         [start]
 *                            |
 *                   ∀x: try both branches
 *                    /              \
 *               x=true            x=false
 *                  |                  |
 *             ∃y: find one       ∃y: find one
 *              /     \            /      \
 *          y=true  y=false    y=true  y=false
 *             |        |         |        |
 *           check    check     check    check
 *           clauses  clauses   clauses  clauses
 *
 *   For SAT: BOTH x branches must have at least one working y value.
 */

#include "QBFSolver.h"
#include <iostream>
#include <algorithm>

// Constructor - initializes solver state
QBFSolver::QBFSolver() : verbose(false), depth(0) {}

// Enable/disable verbose tracing output
void QBFSolver::setVerbose(bool v) {
    verbose = v;
}

// Create indentation string based on recursion depth
std::string QBFSolver::indent() const {
    return std::string(depth * 2, ' ');
}

// Log a message if verbose mode is enabled
void QBFSolver::log(const std::string& msg) const {
    if (verbose) {
        std::cout << indent() << msg << std::endl;
    }
}

/*
 * Main entry point: Initialize solver state and begin recursive search.
 *
 * The preprocessor has already done unit propagation and pure literal
 * elimination. We copy its state and continue with the remaining formula.
 */
Result QBFSolver::solve(const QBFPreprocessor& preprocessor) {
    // Copy state from preprocessor
    quantifierBlocks = preprocessor.getQuantifierBlocks();
    clauses = preprocessor.getClauses();
    assignments = preprocessor.getAssignments();
    depth = 0;

    // Build lookup maps for quick variable info access
    varToQuantifier.clear();
    varToBlockIndex.clear();
    for (size_t i = 0; i < quantifierBlocks.size(); i++) {
        for (int var : quantifierBlocks[i].variables) {
            varToQuantifier[var] = quantifierBlocks[i].type;
            varToBlockIndex[var] = i;
        }
    }

    if (verbose) {
        std::cout << "[SOLVE] Starting with " << clauses.size() << " clauses, "
                  << quantifierBlocks.size() << " quantifier blocks" << std::endl;
    }

    // Check if preprocessing already determined the result
    if (clauses.empty()) {
        log("[RESULT] All clauses satisfied by preprocessing");
        return Result::SAT;
    }
    if (hasEmptyClause()) {
        log("[RESULT] Empty clause found - contradiction");
        return Result::UNSAT;
    }

    // Begin recursive search
    return solve_recursive();
}

/*
 * Check if any clause is empty (all its literals are false).
 * An empty clause means we've hit a contradiction - UNSAT for this branch.
 */
bool QBFSolver::hasEmptyClause() const {
    for (const auto& clause : clauses) {
        if (clause.empty()) {
            return true;
        }
    }
    return false;
}

/*
 * Check if all clauses are satisfied (removed from formula).
 * This happens when every clause has at least one true literal.
 */
bool QBFSolver::allClausesSatisfied() const {
    return clauses.empty();
}

/*
 * Find the next variable to assign, following quantifier prefix order.
 *
 * IMPORTANT: QBF requires processing variables in quantifier order!
 * We can't just pick any unassigned variable - we must respect the
 * prefix: ∀x∃y means we must decide x before y.
 *
 * Returns -1 if all variables are assigned.
 */
int QBFSolver::findNextUnassignedVar() const {
    // Process blocks left to right (outermost to innermost)
    for (const auto& block : quantifierBlocks) {
        for (int var : block.variables) {
            if (assignments.find(var) == assignments.end()) {
                return var;
            }
        }
    }
    return -1;  // All variables assigned
}

/*
 * Assign a value to a variable and record it.
 */
void QBFSolver::assignVariable(int var, bool value) {
    assignments[var] = value;
}

/*
 * Remove a variable's assignment (for backtracking).
 */
void QBFSolver::unassignVariable(int var) {
    assignments.erase(var);
}

/*
 * Simplify clauses based on a variable assignment.
 *
 * When we assign x=true:
 *   - Clauses containing x (positive) are satisfied → remove them
 *   - Clauses containing ¬x have that literal falsified → remove literal
 *
 * An empty clause after simplification means UNSAT.
 */
void QBFSolver::simplifyWithAssignment(int var, bool value) {
    std::vector<Clause> newClauses;

    for (const auto& clause : clauses) {
        bool clauseSatisfied = false;
        Clause newClause;

        for (const auto& lit : clause) {
            if (lit.variable == var) {
                // This literal involves our variable
                // Literal is true if: (var=true and not negated) or (var=false and negated)
                bool litIsTrue = (value != lit.isNegated);
                if (litIsTrue) {
                    clauseSatisfied = true;
                    break;
                }
                // Literal is false - don't add it to new clause
            } else {
                // Literal involves different variable - keep it
                newClause.push_back(lit);
            }
        }

        if (!clauseSatisfied) {
            newClauses.push_back(newClause);
        }
    }

    clauses = newClauses;
}

/*
 * Restore clauses to a previous state (for backtracking).
 */
void QBFSolver::restoreClauses(const std::vector<Clause>& saved) {
    clauses = saved;
}

/*
 * CORE ALGORITHM: Recursive DPLL search for QBF.
 *
 * This is where the magic happens. We recursively try assignments,
 * with different semantics for existential vs universal variables.
 */
Result QBFSolver::solve_recursive() {
    // Base case 1: Empty clause found → contradiction → UNSAT
    if (hasEmptyClause()) {
        log("[CONFLICT] Empty clause - backtracking");
        return Result::UNSAT;
    }

    // Base case 2: All clauses satisfied → SAT
    if (allClausesSatisfied()) {
        log("[SUCCESS] All clauses satisfied");
        return Result::SAT;
    }

    // Find next variable to assign (following quantifier order)
    int var = findNextUnassignedVar();
    if (var == -1) {
        // All variables assigned but clauses remain - check if satisfied
        // (This shouldn't happen with proper simplification)
        return hasEmptyClause() ? Result::UNSAT : Result::SAT;
    }

    // Get variable's quantifier type
    Quantifier qtype = varToQuantifier[var];
    std::string qtypeStr = (qtype == Quantifier::EXISTS) ? "EXISTS" : "FORALL";

    // Save current clause state for backtracking
    std::vector<Clause> savedClauses = clauses;

    depth++;  // Increase indent for verbose output

    if (qtype == Quantifier::EXISTS) {
        /*
         * EXISTENTIAL VARIABLE: EXISTS player's turn
         *
         * We win (SAT) if we can find ANY value that works.
         * Try true first, then false if needed.
         */

        // Try true
        log("[DECIDE] x" + std::to_string(var) + " = true (EXISTS)");
        assignVariable(var, true);
        simplifyWithAssignment(var, true);

        Result result = solve_recursive();
        if (result == Result::SAT) {
            depth--;
            return Result::SAT;  // Found a working value!
        }

        // True didn't work - backtrack and try false
        log("[BACKTRACK] x" + std::to_string(var) + " = true failed, trying false");
        unassignVariable(var);
        restoreClauses(savedClauses);

        log("[DECIDE] x" + std::to_string(var) + " = false (EXISTS)");
        assignVariable(var, false);
        simplifyWithAssignment(var, false);

        result = solve_recursive();
        if (result == Result::SAT) {
            depth--;
            return Result::SAT;  // Found a working value!
        }

        // Neither value works - this branch is UNSAT
        log("[FAIL] x" + std::to_string(var) + " - no value works for EXISTS");
        unassignVariable(var);
        restoreClauses(savedClauses);
        depth--;
        return Result::UNSAT;

    } else {
        /*
         * UNIVERSAL VARIABLE: FORALL player's turn
         *
         * For the formula to be SAT, it must be true for ALL values.
         * So we need BOTH true AND false branches to succeed.
         */

        // Try true
        log("[DECIDE] x" + std::to_string(var) + " = true (FORALL - need both)");
        assignVariable(var, true);
        simplifyWithAssignment(var, true);

        Result result = solve_recursive();
        if (result == Result::UNSAT) {
            // FORALL found a falsifying value - formula is UNSAT
            log("[FAIL] x" + std::to_string(var) + " = true fails - FORALL wins");
            unassignVariable(var);
            restoreClauses(savedClauses);
            depth--;
            return Result::UNSAT;
        }

        // True branch succeeded - now we MUST also check false
        log("[PROGRESS] x" + std::to_string(var) + " = true succeeded, must check false");
        unassignVariable(var);
        restoreClauses(savedClauses);

        log("[DECIDE] x" + std::to_string(var) + " = false (FORALL - need both)");
        assignVariable(var, false);
        simplifyWithAssignment(var, false);

        result = solve_recursive();
        if (result == Result::UNSAT) {
            // FORALL found a falsifying value - formula is UNSAT
            log("[FAIL] x" + std::to_string(var) + " = false fails - FORALL wins");
            unassignVariable(var);
            restoreClauses(savedClauses);
            depth--;
            return Result::UNSAT;
        }

        // BOTH branches succeeded - EXISTS survives this FORALL challenge
        log("[SUCCESS] x" + std::to_string(var) + " - both values work for FORALL");
        depth--;
        return Result::SAT;
    }
}

// Get final variable assignments
const std::unordered_map<int, bool>& QBFSolver::getAssignments() const {
    return assignments;
}
