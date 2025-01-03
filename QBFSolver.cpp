#include "QBFSolver.h"

using namespace std;

bool QBFSolver::evaluateFormula() const {
    for (const auto& clause : clauses) {
        bool clauseSat = false;
        for (const auto& lit : clause) {
            if (assignments.at(lit.variable) != lit.isNegated) {
                clauseSat = true;
                break;
            }
        }
        if (!clauseSat) return false;
    }
    return true;
}

int QBFSolver::getRightmostExistential() const {
    for (int i = quantifierBlocks.size() - 1; i >= 0; --i) {
        if (quantifierBlocks[i].type == Quantifier::EXISTS &&
            !quantifierBlocks[i].variables.empty()) {
            return i + 1;
        }
    }
    return 0;
}

int QBFSolver::getRightmostUniversal() const {
    for (int i = quantifierBlocks.size() - 1; i >= 0; --i) {
        if (quantifierBlocks[i].type == Quantifier::FORALL &&
            !quantifierBlocks[i].variables.empty()) {
            return i + 1;
        }
    }
    return 0;
}

Status QBFSolver::deduce() {
    bool hasEmptyClause = false;
    for (const auto& clause : clauses) {
        if (clause.empty()) {
            hasEmptyClause = true;
            break;
        }
    }

    if (hasEmptyClause) return Status::FALSE;
    if (clauses.empty()) return Status::TRUE;
    return Status::UNKNOWN;
}

Status QBFSolver::solve(const QBFPreprocessor& preprocessor) {
    quantifierBlocks = preprocessor.getQuantifierBlocks();
    clauses = preprocessor.getClauses();
    assignments = preprocessor.getAssignments();
    triedValues.clear();

    Status status = deduce();
    if (status != Status::UNKNOWN) {
        return status;
    }

    int blevel = 1;
    // so this is a vector of integers that
    // maps the end with 0
    vector<int> varIndex(quantifierBlocks.size(), 0); // Track index within each block

    while (true) {
        // if the level is less than 0 or greater than the size of the quantifier blocks
        // then break because we are at edge cases
        if (blevel <= 0 || blevel > quantifierBlocks.size()) {
            break;
        }


        const auto& currentBlock = quantifierBlocks[blevel-1];
        if (varIndex[blevel-1] >= currentBlock.variables.size()) {
            // Move to next block if we've tried all variables in current block
            varIndex[blevel-1] = 0;
            blevel++;
            continue;
        }

        int currentVar = currentBlock.variables[varIndex[blevel-1]];

        if (!triedValues.count(currentVar)) {
            assignments[currentVar] = false;
            triedValues[currentVar] = false;
            status = deduce();

            if (status == Status::UNKNOWN) {
                blevel++;
            } else {
                if ((status == Status::FALSE && currentBlock.type == Quantifier::EXISTS) ||
                    (status == Status::TRUE && currentBlock.type == Quantifier::FORALL)) {
                    // Try the other value
                    assignments[currentVar] = true;
                    triedValues[currentVar] = true;
                    status = deduce();

                    if (status == Status::UNKNOWN) {
                        blevel++;
                    } else {
                        // Both values failed/succeeded - backtrack
                        triedValues.erase(currentVar);
                        assignments.erase(currentVar);
                        varIndex[blevel-1]++;
                        if (currentBlock.type == Quantifier::EXISTS) {
                            blevel = getRightmostExistential();
                        } else {
                            blevel = getRightmostUniversal();
                        }
                    }
                } else {
                    // First value worked - move to next variable
                    varIndex[blevel-1]++;
                    blevel++;
                }
            }
        } else {
            // Reset and move to next variable
            triedValues.erase(currentVar);
            assignments.erase(currentVar);
            varIndex[blevel-1]++;
        }
    }

    return status;
}

const std::unordered_map<int, bool>& QBFSolver::getAssignments() const {
    return assignments;
}
