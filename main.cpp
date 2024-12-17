#include "qbf_preprocessor.h"
#include <iostream>

void printClause(const Clause& clause) {
    std::cout << "(";
    for (size_t i = 0; i < clause.size(); ++i) {
        if (clause[i].isNegated) std::cout << "¬";
        std::cout << "x" << clause[i].variable;
        if (i < clause.size() - 1) std::cout << " ∨ ";
    }
    std::cout << ")";
}

int main() {
    QBFPreprocessor preprocessor;

    // Add quantifier blocks: ∃x1 ∀x2 ∃x3 ∃x4
    preprocessor.addQuantifierBlock(Quantifier::EXISTS, {1});
    preprocessor.addQuantifierBlock(Quantifier::FORALL, {2});
    preprocessor.addQuantifierBlock(Quantifier::EXISTS, {3, 4}); // x3 and x4 in same block

    // Add clauses
    preprocessor.addClause({Literal(1, false), Literal(2, true)});   // (x1 ∨ ¬x2)
    preprocessor.addClause({Literal(1, true), Literal(3, false)});   // (¬x1 ∨ x3)
    preprocessor.addClause({Literal(2, false), Literal(4, false)});  // (x2 ∨ x4)
    preprocessor.addClause({Literal(3, false), Literal(4, false)});  // (x3 ∨ x4)

    std::cout << "Original QBF formula:\n";
    std::cout << "∃x1 ∀x2 ∃x3,x4 ";
    for (const auto& clause : preprocessor.getClauses()) {
        printClause(clause);
        std::cout << " ∧ ";
    }
    std::cout << "\n\n";

    bool result = preprocessor.preprocess();

    std::cout << "\nAfter preprocessing:\n";
    const auto& assignments = preprocessor.getAssignments();
    std::cout << "Number of assignments: " << assignments.size() << "\n";
    if (!assignments.empty()) {
        std::cout << "Assignments:\n";
        for (const auto& [var, value] : assignments) {
            std::cout << "x" << var << " = " << (value ? "true" : "false") << "\n";
        }
    }

    const auto& remainingClauses = preprocessor.getClauses();
    std::cout << "\nNumber of remaining clauses: " << remainingClauses.size() << "\n";
    if (!remainingClauses.empty()) {
        std::cout << "Remaining clauses:\n";
        for (const auto& clause : remainingClauses) {
            printClause(clause);
            std::cout << "\n";
        }
    }

    std::cout << "\nResult: " << (result ? "SATISFIABLE" : "UNSATISFIABLE") << "\n";

    return 0;
}