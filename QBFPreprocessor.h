/*
 * QBFPreprocessor.h - Data Structures and Preprocessing for QBF
 *
 * PREPROCESSING OVERVIEW:
 * =======================
 *
 * Before solving, we simplify the formula using two key techniques:
 *
 * 1. UNIT PROPAGATION
 *    When a clause has only one literal, that literal MUST be true.
 *    Example: If we have clause (x3), then x3 must be true.
 *    We assign x3=true and remove all clauses containing x3.
 *
 * 2. PURE LITERAL ELIMINATION
 *    If a variable appears only positive (or only negative) in all clauses,
 *    we can assign it the satisfying value.
 *    Example: If x5 only appears as x5 (never as ~x5), set x5=true.
 *
 * WHY PREPROCESSING MATTERS:
 * - Reduces the search space dramatically
 * - Can sometimes solve the formula without any search
 * - Makes the remaining problem easier to solve
 *
 * QBF-SPECIFIC CONSIDERATIONS:
 * - We must respect the quantifier prefix order
 * - A universal variable can only be eliminated if earlier variables are assigned
 * - Pure literal rules differ slightly for universal vs existential variables
 */

#ifndef QBF_PREPROCESSOR_H
#define QBF_PREPROCESSOR_H

#include <vector>
#include <unordered_map>
#include <unordered_set>

/*
 * Quantifier types in QBF:
 *   EXISTS (∃): "there exists" - we need to find ONE satisfying value
 *   FORALL (∀): "for all" - must work for ALL values
 */
enum class Quantifier {
    EXISTS,
    FORALL
};

/*
 * A Literal represents a possibly-negated variable.
 *
 * Example:
 *   Literal(3, false) represents x3 (positive)
 *   Literal(3, true)  represents ~x3 (negated)
 */
struct Literal {
    int variable;     // The variable number (positive integer)
    bool isNegated;   // true if this is a negated literal (~x)

    Literal(int var, bool neg);
    Literal complement() const;  // Returns the negated form
    bool operator==(const Literal& other) const;
};

/*
 * A QuantifierBlock groups variables under the same quantifier.
 *
 * In QBF, the prefix alternates between FORALL and EXISTS blocks:
 *   ∀x1,x2 ∃y1,y2 ∀z1 ∃w1 ... (formula)
 *
 * Each block contains one or more variables.
 */
struct QuantifierBlock {
    Quantifier type;              // FORALL or EXISTS
    std::vector<int> variables;   // Variables in this block
};

/*
 * A Clause is a disjunction (OR) of literals.
 *
 * Example: (x1 ∨ ~x2 ∨ x3) is satisfied if ANY of its literals is true.
 *
 * In CNF, we have a conjunction (AND) of clauses:
 *   (x1 ∨ ~x2) ∧ (~x1 ∨ x3) ∧ (x2 ∨ x3)
 *
 * The whole formula is satisfied only if ALL clauses are satisfied.
 */
using Clause = std::vector<Literal>;

/*
 * QBFPreprocessor handles formula storage and preprocessing.
 *
 * It performs simplifications that can reduce the formula size
 * or even solve it completely before the main solver runs.
 */
class QBFPreprocessor {
private:
    std::vector<Clause> clauses;                    // The CNF clauses
    std::vector<QuantifierBlock> quantifierBlocks;  // The quantifier prefix
    std::unordered_map<int, Quantifier> varToQuantifier;  // Quick lookup: var -> quantifier type
    std::unordered_map<int, int> varToBlockIndex;         // Quick lookup: var -> block index
    std::unordered_map<int, bool> assignments;            // Current variable assignments

    // Preprocessing helpers
    bool isPureLiteral(const Literal& lit);
    bool allEarlierVariablesAssigned(int blockIndex) const;
    bool canEliminateVariable(int variable) const;
    bool unitPropagate();
    bool pureLiteralElimination();
    void simplifyClauses();

public:
    // Add quantifier blocks and clauses (called by parser)
    void addQuantifierBlock(Quantifier type, const std::vector<int>& variables);
    void addClause(const Clause& clause);

    // Run preprocessing (unit propagation + pure literal elimination)
    bool preprocess();

    // Access preprocessed state
    const std::unordered_map<int, bool>& getAssignments() const;
    const std::vector<Clause>& getClauses() const;
    const std::vector<QuantifierBlock>& getQuantifierBlocks() const { return quantifierBlocks; }

    // Debug/utility
    void printQuantifierBlock(const QuantifierBlock& block) const;
    bool canPropagateVariable(int var, const std::vector<Clause>& relevantClauses) const;
    std::vector<Clause> getRelevantClauses(int var) const;
};

#endif // QBF_PREPROCESSOR_H
