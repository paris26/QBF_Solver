#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

using namespace std;


struct Literal{
    int variable;
    bool isNegated;

    Literal(int var, bool neg ) : variable(var), isNegated(neg) {}

    Literal complement() const{
        return Literal(variable, !isNegated);
    }
};


using Clause = vector<Literal>;

class QBF_Preprocessor{
    private:
        vector<Clause> clauses;
        unordered_set<int> variables;
        unordered_map<int, bool> assignments;


        bool isPureLiteral(const Literal& lit){
            bool FoundLit = false;
            Literal comp = lit.complement();

            for (const auto& clause : clauses){
                for(const auto& current_lit : clause){
                    if(current_lit.variable == lit.variable){
                        if(current_lit.isNegated == lit.isNegated){
                            return false;
                        }
                        FoundLit = true;
                    }
                }
            }
            return FoundLit;
        };

        bool unitPropagate() {
            bool changed = false; 
            bool foundUnit;

            do{
                foundUnit = false; 

                for (const auto& clause : clauses)
                {
                    if( clause.size() == 1){
                        const Literal& unit = clause[0];
                        assignments[unit.variable] = !unit.isNegated;

                          auto newEnd = std::remove_if(clauses.begin(), clauses.end(),
                        [&](const Clause& c) {
                            return std::any_of(c.begin(), c.end(),
                                [&](const Literal& l) {
                                    return l.variable == unit.variable &&
                                           l.isNegated == unit.isNegated;
                                });
                        });
                        
                        clauses.erase(newEnd, clauses.end());

                    }
                }
                
            }
        }



};