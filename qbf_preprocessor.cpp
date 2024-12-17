#include "qbf_preprocessor.h"

bool QBFPreprocessor::isPureLiteral(const Literal& lit) {
    bool foundLit = false;
    Literal comp = lit.complement();
    
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

bool QBFPreprocessor::unitPropagate() {
    bool changed = false;
    bool foundUnit;
    
    do {
        foundUnit = false;
        
        for (const auto& clause : clauses) {
            if (clause.size() == 1) {
                const Literal& unit = clause[0];
                int unitBlockIndex = varToBlockIndex[unit.variable];
                
                // Check if propagation is allowed
                bool canPropagate = true;
                for (const auto& lit : clause) {
                    if (varToBlockIndex[lit.variable] > unitBlockIndex) {
                        canPropagate = false;
                        break;
                    }
                }
                
                if (canPropagate) {
                    assignments[unit.variable] = !unit.isNegated;
                    
                    // Remove clauses containing the literal
                    auto newEnd = std::remove_if(clauses.begin(), clauses.end(),
                        [&](const Clause& c) {
                            return std::any_of(c.begin(), c.end(),
                                [&](const Literal& l) {
                                    return l.variable == unit.variable &&
                                           l.isNegated == unit.isNegated;
                                });
                        });
                    clauses.erase(newEnd, clauses.end());
                    
                    // Remove complementary literals
                    for (auto& c : clauses) {
                        auto litEnd = std::remove_if(c.begin(), c.end(),
                            [&](const Literal& l) {
                                return l.variable == unit.variable &&
                                       l.isNegated != unit.isNegated;
                            });
                        c.erase(litEnd, c.end());
                    }
                    
                    foundUnit = true;
                    changed = true;
                    break;
                }
            }
        }
    } while (foundUnit);
    
    return changed;
}

bool QBFPreprocessor::pureLiteralElimination() {
    bool changed = false;
    
    for (const auto& block : quantifierBlocks) {
        for (int var : block.variables) {
            if (assignments.count(var)) continue;
            
            Literal posLit(var, false);
            Literal negLit(var, true);
            
            if (isPureLiteral(posLit)) {
                assignments[var] = (block.type == Quantifier::EXISTS);
                changed = true;
            }
            else if (isPureLiteral(negLit)) {
                assignments[var] = (block.type == Quantifier::FORALL);
                changed = true;
            }
        }
    }
    
    if (changed) {
        simplifyClauses();
    }
    
    return changed;
}

void QBFPreprocessor::simplifyClauses() {
    auto newEnd = std::remove_if(clauses.begin(), clauses.end(),
        [&](const Clause& clause) {
            for (const auto& lit : clause) {
                if (assignments.count(lit.variable) &&
                    assignments[lit.variable] != lit.isNegated) {
                    return true;  // Clause is satisfied
                }
            }
            return false;
        });
    clauses.erase(newEnd, clauses.end());
    
    for (auto& clause : clauses) {
        auto litEnd = std::remove_if(clause.begin(), clause.end(),
            [&](const Literal& lit) {
                return assignments.count(lit.variable) &&
                       assignments[lit.variable] == lit.isNegated;
            });
        clause.erase(litEnd, clause.end());
    }
}

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
    do {
        changed = false;
        changed |= unitPropagate();
        changed |= pureLiteralElimination();
    } while (changed);
    
    // If all clauses are satisfied (empty) OR we have remaining clauses, it's potentially satisfiable
    return true;
}

const std::unordered_map<int, bool>& QBFPreprocessor::getAssignments() const {
    return assignments;
}

const std::vector<Clause>& QBFPreprocessor::getClauses() const {
    return clauses;
}