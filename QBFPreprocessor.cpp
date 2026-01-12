/*
 * QBFPreprocessor.cpp - Preprocessing Implementation for QBF
 *
 * This file implements preprocessing techniques that simplify the formula
 * before the main search begins. These techniques can dramatically reduce
 * the search space.
 *
 * TECHNIQUES IMPLEMENTED:
 *
 * 1. Unit Propagation
 *    - Find clauses with only one literal (unit clauses)
 *    - That literal MUST be true for the formula to be satisfiable
 *    - Assign the variable and simplify the formula
 *
 * 2. Pure Literal Elimination
 *    - Find variables that appear with only one polarity (only positive or only negative)
 *    - Assign them the satisfying value
 *    - For existentials: pick the value that satisfies clauses
 *    - For universals: safe to eliminate if no dependency issues
 */

#include "QBFPreprocessor.h"
#include <algorithm>
#include <string>
#include <iostream>
#include <vector>

// ============================================================================
// Literal Implementation
// ============================================================================

Literal::Literal(int var, bool neg) : variable(var), isNegated(neg) {}

/*
 * Return the complementary literal.
 * If this is x, return ~x. If this is ~x, return x.
 */
Literal Literal::complement() const {
    return Literal(variable, !isNegated);
}

bool Literal::operator==(const Literal& other) const {
    return variable == other.variable && isNegated == other.isNegated;
}

// ============================================================================
// Debug/Utility Functions
// ============================================================================

/*
 * Print a quantifier block for debugging.
 * Example output: "FORALL X1, X2, X3"
 */
void QBFPreprocessor::printQuantifierBlock(const QuantifierBlock& block) const {
    std::cout << (block.type == Quantifier::FORALL ? "FORALL" : "EXISTS") << " ";
    for (size_t i = 0; i < block.variables.size(); i++) {
        std::cout << "X" << block.variables[i];
        if (i < block.variables.size() - 1) std::cout << ", ";
    }
}

// ============================================================================
// Pure Literal Detection
// ============================================================================

/*
 * Check if a literal is "pure" - appears only in one polarity.
 *
 * A literal is pure if its complement never appears in any clause.
 * Example: If x3 appears but ~x3 never does, then x3 is pure.
 *
 * Pure literals are important because:
 * - For existential vars: we can always choose the satisfying value
 * - For universal vars: if pure, it doesn't constrain the formula
 */
bool QBFPreprocessor::isPureLiteral(const Literal& lit) {
    bool foundLit = false;
    for (const auto& clause : clauses) {
        for (const auto& currLit : clause) {
            if (currLit.variable == lit.variable) {
                // Found the same variable - check polarity
                if (currLit.isNegated != lit.isNegated) {
                    return false;  // Found complement, not pure
                }
                foundLit = true;
            }
        }
    }
    return foundLit;  // Pure if found and no complement exists
}

// ============================================================================
// Dependency Checking for QBF
// ============================================================================

/*
 * Check if all variables in earlier quantifier blocks are assigned.
 *
 * In QBF, we must respect the quantifier ordering. A variable in block i
 * can only be safely assigned if all variables in blocks 0..i-1 are already
 * determined (either assigned or irrelevant to remaining clauses).
 */
bool QBFPreprocessor::allEarlierVariablesAssigned(int blockIndex) const {
    for (int i = 0; i < blockIndex; i++) {
        for (int var : quantifierBlocks[i].variables) {
            if (assignments.find(var) == assignments.end()) {
                return false;  // Earlier variable still unassigned
            }
        }
    }
    return true;
}

/*
 * Check if a variable can be safely eliminated during preprocessing.
 * This requires all earlier variables to be assigned.
 */
bool QBFPreprocessor::canEliminateVariable(int variable) const {
    int blockIndex = varToBlockIndex.at(variable);
    return allEarlierVariablesAssigned(blockIndex);
}

/*
 * Check if we can propagate a unit literal for a specific variable.
 *
 * For QBF, propagation rules are more complex than SAT:
 *
 * EXISTENTIAL variables: Can propagate if all earlier universal variables
 * that appear in the same clauses are already assigned.
 *
 * UNIVERSAL variables: Can propagate if no later existential variables
 * in the same clauses are unassigned.
 *
 * These rules ensure we don't make invalid inferences that violate
 * the quantifier semantics.
 */
bool QBFPreprocessor::canPropagateVariable(int var, const std::vector<Clause>& relevantClauses) const {
    int varBlockIndex = varToBlockIndex.at(var);
    Quantifier varQuantifier = varToQuantifier.at(var);

    if (varQuantifier == Quantifier::EXISTS) {
        // For existential variables:
        // Check that no earlier universal variables are unassigned
        for (const auto& clause : relevantClauses) {
            for (const auto& lit : clause) {
                if (lit.variable == var) continue;

                int litBlockIndex = varToBlockIndex.at(lit.variable);
                // If there's an earlier unassigned universal, we can't propagate
                if (litBlockIndex < varBlockIndex &&
                    varToQuantifier.at(lit.variable) == Quantifier::FORALL &&
                    assignments.count(lit.variable) == 0) {
                    return false;
                }
            }
        }
        return true;
    }

    if (varQuantifier == Quantifier::FORALL) {
        // For universal variables:
        // Check that no later existential variables are unassigned
        for (const auto& clause : relevantClauses) {
            for (const auto& lit : clause) {
                if (lit.variable == var) continue;

                int litBlockIndex = varToBlockIndex.at(lit.variable);
                // If there's a later unassigned existential, we can't propagate
                if (litBlockIndex > varBlockIndex &&
                    varToQuantifier.at(lit.variable) == Quantifier::EXISTS &&
                    assignments.count(lit.variable) == 0) {
                    return false;
                }
            }
        }
        return true;
    }

    return false;
}

/*
 * Find all clauses that contain a given variable.
 */
std::vector<Clause> QBFPreprocessor::getRelevantClauses(int var) const {
    std::vector<Clause> relevant;
    for (const auto& clause : clauses) {
        for (const auto& lit : clause) {
            if (lit.variable == var) {
                relevant.push_back(clause);
                break;
            }
        }
    }
    return relevant;
}

// ============================================================================
// Unit Propagation
// ============================================================================

/*
 * Perform unit propagation: find and process unit clauses.
 *
 * A unit clause has exactly one literal. That literal MUST be true,
 * otherwise the clause (and thus the whole formula) would be false.
 *
 * Process:
 * 1. Find all unit clauses
 * 2. Sort by block index (process innermost first for QBF safety)
 * 3. For each propagatable unit:
 *    - Assign the variable
 *    - Remove satisfied clauses
 *    - Remove falsified literals from other clauses
 * 4. Repeat until no more units found
 *
 * Returns true if any propagation was performed.
 */
bool QBFPreprocessor::unitPropagate() {
    bool changed = false;
    bool foundUnit;

    do {
        foundUnit = false;

        // Collect all unit clauses with their block indices
        std::vector<std::pair<Literal, int>> unitLiterals;
        for (const auto& clause : clauses) {
            if (clause.size() == 1) {
                const Literal& unit = clause[0];
                int blockIndex = varToBlockIndex[unit.variable];
                unitLiterals.push_back({unit, blockIndex});
            }
        }

        // Sort by block index descending (process innermost/later blocks first)
        // This is safer for QBF because inner variables have fewer dependencies
        std::sort(unitLiterals.begin(), unitLiterals.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });

        for (const auto& [unit, blockIndex] : unitLiterals) {
            // Skip if already assigned
            if (assignments.count(unit.variable) > 0) continue;

            // Check if we can safely propagate this variable
            auto relevantClauses = getRelevantClauses(unit.variable);
            if (canPropagateVariable(unit.variable, relevantClauses)) {
                // Assign: if literal is positive, var=true; if negated, var=false
                assignments[unit.variable] = !unit.isNegated;

                // Remove clauses satisfied by this assignment
                auto newEnd = std::remove_if(clauses.begin(), clauses.end(),
                    [&](const Clause& c) {
                        return std::any_of(c.begin(), c.end(),
                            [&](const Literal& l) {
                                return l.variable == unit.variable &&
                                       l.isNegated == unit.isNegated;
                            });
                    });
                clauses.erase(newEnd, clauses.end());

                // Remove falsified literals from remaining clauses
                for (auto& c : clauses) {
                    auto litEnd = std::remove_if(c.begin(), c.end(),
                        [&](const Literal& l) {
                            return l.variable == unit.variable &&
                                   l.isNegated != unit.isNegated;
                        });
                    c.erase(litEnd, c.end());

                    // Check if we created a new unit clause
                    if (c.size() == 1) {
                        foundUnit = true;
                    }
                }

                changed = true;
                foundUnit = true;
                break;  // Restart to find new units
            }
        }
    } while (foundUnit);

    return changed;
}

// ============================================================================
// Pure Literal Elimination
// ============================================================================

/*
 * Perform pure literal elimination.
 *
 * A pure literal appears with only one polarity in all clauses.
 * We can assign it the satisfying value:
 * - If x is pure (never ~x), set x=true
 * - If ~x is pure (never x), set x=false
 *
 * Returns true if any elimination was performed.
 */
bool QBFPreprocessor::pureLiteralElimination() {
    bool changed = false;
    std::vector<std::pair<int, bool>> assignments_to_make;

    // Process blocks from innermost to outermost
    for (int blockIndex = quantifierBlocks.size() - 1; blockIndex >= 0; --blockIndex) {
        const auto& block = quantifierBlocks[blockIndex];

        for (int var : block.variables) {
            // Skip already assigned variables
            if (assignments.count(var)) continue;
            // Skip if we can't safely eliminate
            if (!canEliminateVariable(var)) continue;

            Literal posLit(var, false);  // x
            Literal negLit(var, true);   // ~x

            bool posIsPure = isPureLiteral(posLit);
            bool negIsPure = isPureLiteral(negLit);

            if (posIsPure || negIsPure) {
                // Assign the satisfying value
                // If positive is pure, set true; if negative is pure, set false
                bool assignment = posIsPure;
                assignments_to_make.emplace_back(var, assignment);
                changed = true;
            }
        }
    }

    // Apply all assignments
    for (const auto& [var, value] : assignments_to_make) {
        assignments[var] = value;
    }

    // Simplify clauses based on new assignments
    if (changed) {
        simplifyClauses();
    }

    return changed;
}

// ============================================================================
// Clause Simplification
// ============================================================================

/*
 * Simplify clauses based on current assignments.
 *
 * For each clause:
 * - If any literal is true under current assignment, remove the clause (satisfied)
 * - If a literal is false, remove it from the clause
 * - If a clause becomes empty, keep it (indicates UNSAT)
 */
void QBFPreprocessor::simplifyClauses() {
    std::vector<Clause> newClauses;

    for (const auto& clause : clauses) {
        bool isClauseSatisfied = false;
        Clause newClause;

        for (const auto& lit : clause) {
            if (assignments.count(lit.variable)) {
                // Variable is assigned - check if literal is satisfied
                bool literalIsTrue = (assignments[lit.variable] != lit.isNegated);
                if (literalIsTrue) {
                    isClauseSatisfied = true;
                    break;
                }
                // Literal is false - don't add it
            } else {
                // Variable not assigned - keep the literal
                newClause.push_back(lit);
            }
        }

        if (!isClauseSatisfied) {
            if (newClause.empty()) {
                // Empty clause = contradiction = UNSAT
                // Keep just the empty clause to signal this
                newClauses.clear();
                newClauses.push_back(newClause);
                break;
            }
            newClauses.push_back(newClause);
        }
    }

    clauses = newClauses;
}

// ============================================================================
// Public Interface
// ============================================================================

/*
 * Add a quantifier block to the formula.
 * Blocks should be added in prefix order (outermost first).
 */
void QBFPreprocessor::addQuantifierBlock(Quantifier type, const std::vector<int>& variables) {
    int blockIndex = quantifierBlocks.size();
    quantifierBlocks.push_back({type, variables});

    // Update lookup maps
    for (int var : variables) {
        varToQuantifier[var] = type;
        varToBlockIndex[var] = blockIndex;
    }
}

/*
 * Add a clause to the formula.
 */
void QBFPreprocessor::addClause(const Clause& clause) {
    clauses.push_back(clause);
}

/*
 * Run all preprocessing steps until no more simplifications possible.
 *
 * Returns true if the formula is potentially satisfiable,
 * false if we detected UNSAT (empty clause found).
 */
bool QBFPreprocessor::preprocess() {
    bool changed;
    bool hasEmptyClause = false;

    do {
        changed = false;

        // Check for empty clause (UNSAT indicator)
        for (const auto& clause : clauses) {
            if (clause.empty()) {
                hasEmptyClause = true;
                break;
            }
        }
        if (hasEmptyClause) {
            break;  // Formula is UNSAT
        }

        // Apply preprocessing techniques
        changed |= unitPropagate();
        changed |= pureLiteralElimination();

    } while (changed);

    // Return true if no empty clause found
    // (empty clauses vector = all satisfied = SAT)
    return clauses.empty() || (!hasEmptyClause && !clauses.empty());
}

const std::unordered_map<int, bool>& QBFPreprocessor::getAssignments() const {
    return assignments;
}

const std::vector<Clause>& QBFPreprocessor::getClauses() const {
    return clauses;
}
