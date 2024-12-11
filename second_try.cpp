#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

enum class Quantifier {
    EXISTS,
    FORALL
};

// Structure to represent a literal in the QBF formula
struct Literal {
    int variable;
    bool isNegated;
    
    Literal(int var, bool neg) : variable(var), isNegated(neg) {}
    
    Literal complement() const {
        return Literal(variable, !isNegated);
    }
};

// Structure for quantifier blocks
struct QuantifierBlock {
    Quantifier type;
    std::vector<int> variables;
};

using Clause = std::vector<Literal>;

class QBFPreprocessor {
private:
    std::vector<Clause> clauses;
    std::vector<QuantifierBlock> quantifierBlocks;
    std::unordered_map<int, Quantifier> varToQuantifier;
    std::unordered_map<int, int> varToBlockIndex;
    std::unordered_map<int, bool> assignments;

    // Check if literal appears pure in its quantifier block
    bool isPureLiteral(const Literal& lit) {
        bool foundLit = false ;
        Literal comp = lit.complement();
        
        // Get the quantifier block for this variable
        Quantifier quantifier = varToQuantifier[lit.variable];
        
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

    // Apply unit propagation respecting quantifier ordering
    bool unitPropagate() {
        bool changed = false;
        bool foundUnit;
        
        do {
            foundUnit = false;
            
            for (const auto& clause : clauses) {
                if (clause.size() == 1) {
                    const Literal& unit = clause[0];
                    int unitBlockIndex = varToBlockIndex[unit.variable];
                    
                    // Check if propagation is allowed (all variables in clause are in same or earlier blocks)
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
                        
                        // Remove complementary literals from remaining clauses
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

    // Apply pure literal elimination considering quantifiers
    bool pureLiteralElimination() {
        bool changed = false;
        
        // Process each quantifier block separately
        for (const auto& block : quantifierBlocks) {
            for (int var : block.variables) {
                if (assignments.count(var)) continue;
                
                Literal posLit(var, false);
                Literal negLit(var, true);
                
                if (isPureLiteral(posLit)) {
                    // For existential, assign true if positive is pure
                    // For universal, assign false if negative is pure
                    assignments[var] = (block.type == Quantifier::EXISTS);
                    changed = true;
                }
                else if (isPureLiteral(negLit)) {
                    // For existential, assign false if negative is pure
                    // For universal, assign true if positive is pure
                    assignments[var] = (block.type == Quantifier::FORALL);
                    changed = true;
                }
            }
        }
        
        if (changed) {
            // Update clauses based on assignments
            simplifyClauses();
        }
        
        return changed;
    }

    // Helper to simplify clauses based on current assignments
    void simplifyClauses() {
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

public:
    // Add a quantifier block
    void addQuantifierBlock(Quantifier type, const std::vector<int>& variables) {
        int blockIndex = quantifierBlocks.size();
        quantifierBlocks.push_back({type, variables});
        
        // Update mappings
        for (int var : variables) {
            varToQuantifier[var] = type;
            varToBlockIndex[var] = blockIndex;
        }
    }
    
    void addClause(const Clause& clause) {
        clauses.push_back(clause);
    }
    
    bool preprocess() {
        bool changed;
        do {
            changed = false;
            changed |= unitPropagate();
            changed |= pureLiteralElimination();
        } while (changed);
        
        return !clauses.empty();
    }
    
    const std::unordered_map<int, bool>& getAssignments() const {
        return assignments;
    }
    
    const std::vector<Clause>& getClauses() const {
        return clauses;
    }
};
