#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include "QBFPreprocessor.h"
// #include "QBFSolver.h"

//reading the file
#include <fstream>
#include <sstream>
using namespace std;

void printClause(const Clause& clause) {
    std::cout << "(";
    for (size_t i = 0; i < clause.size(); ++i) {
        if (clause[i].isNegated) std::cout << "¬";
        std::cout << "x" << clause[i].variable;
        if (i < clause.size() - 1) std::cout << " ∨ ";
    }
    std::cout << ")";
}

void printFormula(const std::vector<Clause>& clauses) {
    for (size_t i = 0; i < clauses.size(); ++i) {
        printClause(clauses[i]);
        if (i < clauses.size() - 1) std::cout << " ∧ ";
    }
    std::cout << std::endl;
}

// void runFormula(const std::string& description, QBFPreprocessor& preprocessor) {
//     QBFSolver solver;
//     Status result = solver.solve(preprocessor);
//     std::cout << "Result: " << (result == Status::TRUE ? "SAT" :
//                                result == Status::FALSE ? "UNSAT" : "UNKNOWN") << std::endl;
// }

void readQBF(QBFPreprocessor& preprocessor){
    ifstream file("formula.txt");
    string content;
    string line;

    while(getline(file, line)){
        istringstream iss(line);


        char type;
        iss >> type;

        if(line.empty() || type == 'c' || type == 'p'){
            continue;
        }

        if( type == 'a' || type == 'e'){
            vector<int> variables;
            int var;

            while( iss >> var && var != 0){
                variables.push_back(var);
            }
            preprocessor.addQuantifierBlock(type == 'a' ? Quantifier::FORALL : Quantifier::EXISTS, variables);
        } else {
            vector<Literal> clause;
            int var;

            while(iss >> var && var!=0){
                bool isNegated = var < 0;
                clause.push_back({abs(var), isNegated});
            }
            preprocessor.addClause(clause);
        }
    }
    file.close();
}

int main() {
    QBFPreprocessor preprocessor;
    readQBF(preprocessor);

    const auto& blocks = preprocessor.getQuantifierBlocks();
    for(const auto& block : blocks ){
        preprocessor.printQuantifierBlock(block);
        cout << endl;
    }

    const auto& clauses = preprocessor.getClauses();
    printFormula(clauses);

    return 0;
}
