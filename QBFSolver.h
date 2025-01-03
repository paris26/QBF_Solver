#ifndef QBF_SOLVER_H
#define QBF_SOLVER_H

#include "QBFPreprocessor.h"
#include <unordered_map>

enum class Status { TRUE, FALSE, UNKNOWN };

class QBFSolver {
private:
    std::vector<QuantifierBlock> quantifierBlocks;
    std::vector<Clause> clauses;
    std::unordered_map<int, bool> assignments;
    std::unordered_map<int, bool> triedValues;

    bool evaluateFormula() const;
    int getRightmostExistential() const;
    int getRightmostUniversal() const;
    Status deduce();

public:
    Status solve(const QBFPreprocessor& preprocessor);
    const std::unordered_map<int, bool>& getAssignments() const;
};

#endif // QBF_SOLVER_H
