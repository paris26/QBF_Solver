#ifndef QBF_PREPROCESSOR_H
#define QBF_PREPROCESSOR_H

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

using namespace std;

enum class Quantifier {
    EXISTS,
    FORALL
};

struct Literal {
    int variable;
    bool isNegated;
    
    Literal(int var, bool neg) : variable(var), isNegated(neg) {}
    
    Literal complement() const {
        return Literal(variable, !isNegated);
    }
};

struct QuantifierBlock {
    Quantifier type;
    vector<int> variables;
};

using Clause = std::vector<Literal>;

class QBFPreprocessor {
private:
    vector<Clause> clauses;
    vector<QuantifierBlock> quantifierBlocks;
    unordered_map<int, Quantifier> varToQuantifier;
    unordered_map<int, int> varToBlockIndex;
    unordered_map<int, bool> assignments;

    bool isPureLiteral(const Literal& lit);
    bool unitPropagate();
    bool pureLiteralElimination();
    void simplifyClauses();

public:
    void addQuantifierBlock(Quantifier type, const vector<int>& variables);
    void addClause(const Clause& clause);
    bool preprocess();
    const unordered_map<int, bool>& getAssignments() const;
    const vector<Clause>& getClauses() const;
};

#endif