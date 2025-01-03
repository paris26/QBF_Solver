#include "QBFPreprocessor.h"
#include <algorithm>
#include <string>
#include <iostream>
#include <vector>


using namespace std;

// Literal implementation
Literal::Literal(int var, bool neg) : variable(var), isNegated(neg) {}

Literal Literal::complement() const {
    return Literal(variable, !isNegated);
}

bool Literal::operator==(const Literal& other) const {
    return variable == other.variable && isNegated == other.isNegated;
}


void QBFPreprocessor::printQuantifierBlock(const QuantifierBlock& QuantifierBlock) const{
    cout << (QuantifierBlock.type == Quantifier::FORALL ? "FORALL" : "EXISTS") << " ";
    for( size_t i=0; i<QuantifierBlock.variables.size(); i++){
        cout << "X" << QuantifierBlock.variables[i];
        if(i < (QuantifierBlock.variables.size() - 1)) cout << ", ";
    }
}

// QBFPreprocessor private methods
bool QBFPreprocessor::isPureLiteral(const Literal& lit) {
    bool foundLit = false;
    for (const auto& clause : clauses) {
        for (const auto& currLit : clause) {
            if (currLit.variable == lit.variable) {
                if (currLit.isNegated != lit.isNegated) {
                    return false;  // Found complement, not pure
                }
                foundLit = true;
            }
        }
    }
    return foundLit;
}

// this just checks if all earlier vatables are assigned
// so we can assign the current variable
bool QBFPreprocessor::allEarlierVariablesAssigned(int blockIndex) const {
    for (int i = 0; i < blockIndex; i++) {
        for (int var : quantifierBlocks[i].variables) {
            if (assignments.find(var) == assignments.end()) {
                return false;
            }
        }
    }
    return true;
}

bool QBFPreprocessor::canEliminateVariable(int variable) const {
    int blockIndex = varToBlockIndex.at(variable);
    return allEarlierVariablesAssigned(blockIndex);
}

// Helper method to check if we can propagate a variable based on dependencies
bool QBFPreprocessor::canPropagateVariable(int var, const std::vector<Clause>& relevantClauses) const {
    int varBlockIndex = varToBlockIndex.at(var);
    Quantifier varQuantifier = varToQuantifier.at(var);

    // For existential variables, we can propagate if:
    // 1. All universal variables it depends on are in later blocks, or
    // 2. All universal variables it depends on in earlier blocks are already assigned
    if (varQuantifier == Quantifier::EXISTS) {
        for (const auto& clause : relevantClauses) {
            for (const auto& lit : clause) {
                if (lit.variable == var) continue;

                int litBlockIndex = varToBlockIndex.at(lit.variable);
                if (litBlockIndex < varBlockIndex &&
                    varToQuantifier.at(lit.variable) == Quantifier::FORALL &&
                    assignments.count(lit.variable) == 0) {
                    return false;
                }
            }
        }
        return true;
    }

    // For universal variables, we can propagate if:
    // 1. No existential variables from earlier blocks are unassigned
    // 2. Or if we're forced to by a unit clause that doesn't depend on later existentials
    if (varQuantifier == Quantifier::FORALL) {
        for (const auto& clause : relevantClauses) {
            for (const auto& lit : clause) {
                if (lit.variable == var) continue;

                int litBlockIndex = varToBlockIndex.at(lit.variable);
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

// Helper method to find all clauses relevant to a variable
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

bool QBFPreprocessor::unitPropagate() {
    bool changed = false;
    bool foundUnit;

    do {
        foundUnit = false;

        // Collect all unit clauses and sort by block index (reverse order)
        std::vector<std::pair<Literal, int>> unitLiterals;
        for (const auto& clause : clauses) {
            if (clause.size() == 1) {
                const Literal& unit = clause[0];
                int blockIndex = varToBlockIndex[unit.variable];
                unitLiterals.push_back({unit, blockIndex});
            }
        }

        // Sort by block index in descending order (process later blocks first)
        std::sort(unitLiterals.begin(), unitLiterals.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });

        for (const auto& [unit, blockIndex] : unitLiterals) {
            if (assignments.count(unit.variable) > 0) continue;

            // Get relevant clauses for this variable
            auto relevantClauses = getRelevantClauses(unit.variable);

            // Check if we can propagate this variable
            if (canPropagateVariable(unit.variable, relevantClauses)) {
                // Perform the propagation
                assignments[unit.variable] = !unit.isNegated;

                // Remove satisfied clauses
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

                    // If we created a new unit clause, process it in the next iteration
                    if (c.size() == 1) {
                        foundUnit = true;
                    }
                }

                changed = true;
                foundUnit = true;
                break;
            }
        }
    } while (foundUnit);

    return changed;
}

bool QBFPreprocessor::pureLiteralElimination() {
    bool changed = false;
    std::vector<std::pair<int, bool>> assignments_to_make;

    for (int blockIndex = quantifierBlocks.size() - 1; blockIndex >= 0; --blockIndex) {
        const auto& block = quantifierBlocks[blockIndex];

        for (int var : block.variables) {
            if (assignments.count(var)) continue;
            if (!canEliminateVariable(var)) continue;

            Literal posLit(var, false);
            Literal negLit(var, true);

            bool posIsPure = isPureLiteral(posLit);
            bool negIsPure = isPureLiteral(negLit);

            if (posIsPure || negIsPure) {
                bool assignment;
                if (block.type == Quantifier::EXISTS) {
                    assignment = posIsPure;
                } else {
                    // For universal variables, choose satisfying value
                    assignment = posIsPure;
                }
                assignments_to_make.emplace_back(var, assignment);
                changed = true;
            }
        }
    }

    for (const auto& [var, value] : assignments_to_make) {
        assignments[var] = value;
    }

    if (changed) {
        simplifyClauses();
    }

    return changed;
}

void QBFPreprocessor::simplifyClauses() {
    std::vector<Clause> newClauses;

    for (const auto& clause : clauses) {
        bool isClauseSatisfied = false;
        Clause newClause;

        for (const auto& lit : clause) {
            if (assignments.count(lit.variable)) {
                if (assignments[lit.variable] != lit.isNegated) {
                    isClauseSatisfied = true;
                    break;
                }
            } else {
                newClause.push_back(lit);
            }
        }

        // Only add the clause if it's not satisfied
        if (!isClauseSatisfied) {
            if (newClause.empty()) {
                // Empty clause after simplification means UNSAT
                newClauses.clear();
                newClauses.push_back(newClause);  // Add empty clause to indicate UNSAT
                break;
            }
            newClauses.push_back(newClause);
        }
    }

    clauses = newClauses;
}
// Public methods remain the same...
void QBFPreprocessor::addQuantifierBlock(Quantifier type, const std::vector<int>& variables) {
    int blockIndex = quantifierBlocks.size();
    quantifierBlocks.push_back({type, variables});

    for (int var : variables) {
        varToQuantifier[var] = type;
        varToBlockIndex[var] = blockIndex;
    }
}

void QBFPreprocessor::addClause(const Clause& clause) {
    clauses.push_back(clause);
}

bool QBFPreprocessor::preprocess() {
    bool changed;
    bool hasEmptyClause = false;  // Track if we've found an empty clause

    do {
        changed = false;

        // Check for empty clause before preprocessing
        for (const auto& clause : clauses) {
            if (clause.empty()) {
                hasEmptyClause = true;
                break;
            }
        }
        if (hasEmptyClause) {
            break;  // Found empty clause - formula is UNSAT
        }

        changed |= unitPropagate();
        changed |= pureLiteralElimination();

    } while (changed);

    // If all clauses are satisfied (clauses.empty()) and we haven't found an empty clause,
    // the formula is SAT, so we should return true
    return clauses.empty() || (!hasEmptyClause && !clauses.empty());
}

const std::unordered_map<int, bool>& QBFPreprocessor::getAssignments() const {
    return assignments;
}

const std::vector<Clause>& QBFPreprocessor::getClauses() const {
    return clauses;
}
