#ifndef QBF_PREPROCESSOR_H
#define QBF_PREPROCESSOR_H

#include <vector>
#include <unordered_map>
#include <unordered_set>

using namespace std;

enum class Quantifier {
    EXISTS,
    FORALL
};

struct Literal {
    int variable;
    bool isNegated;

    Literal(int var, bool neg);
    Literal complement() const;
    bool operator==(const Literal& other) const;
};

struct QuantifierBlock {
    Quantifier type; // FORALL or EXISTS
    vector<int> variables;
};

// so clause is a vector of literals
using Clause = vector<Literal>;


class QBFPreprocessor {
private:
// we have a vector of a vector of literals
    vector<Clause> clauses;
    vector<QuantifierBlock> quantifierBlocks;
    unordered_map<int, Quantifier> varToQuantifier;
    unordered_map<int, int> varToBlockIndex;
    unordered_map<int, bool> assignments;

    bool isPureLiteral(const Literal& lit);
    bool allEarlierVariablesAssigned(int blockIndex) const;
    bool canEliminateVariable(int variable) const;
    bool unitPropagate();
    bool pureLiteralElimination();
    void simplifyClauses();

public:
    void printQuantifierBlock(const QuantifierBlock& block) const;
    void addQuantifierBlock(Quantifier type, const std::vector<int>& variables);
    void addClause(const Clause& clause);
    bool preprocess();
    const std::unordered_map<int, bool>& getAssignments() const;
    const std::vector<Clause>& getClauses() const;
    bool canPropagateVariable(int var, const std::vector<Clause>& relevantClauses) const;
    vector<Clause> getRelevantClauses(int var) const;
    const vector<QuantifierBlock>& getQuantifierBlocks() const { return quantifierBlocks; }
};

#endif // QBF_PREPROCESSOR_H
