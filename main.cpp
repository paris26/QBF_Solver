/*
 * main.cpp - QBF Solver Entry Point
 *
 * This is an educational QBF (Quantified Boolean Formula) solver.
 *
 * USAGE:
 *   ./qbf <formula.qdimacs>           Solve the formula
 *   ./qbf -v <formula.qdimacs>        Solve with verbose tracing (educational mode)
 *
 * The solver reads formulas in QDIMACS format, a standard format for QBF.
 * Use -v to see step-by-step how the algorithm explores the search tree.
 */

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "QBFPreprocessor.h"
#include "QBFSolver.h"

/*
 * Print a single clause in human-readable form.
 * Example: (x1 v ~x2 v x3)
 */
void printClause(const Clause& clause) {
    std::cout << "(";
    for (size_t i = 0; i < clause.size(); ++i) {
        if (clause[i].isNegated) std::cout << "\302\254";  // Unicode NOT symbol
        std::cout << "x" << clause[i].variable;
        if (i < clause.size() - 1) std::cout << " \342\210\250 ";  // Unicode OR symbol
    }
    std::cout << ")";
}

/*
 * Print the entire CNF formula as a conjunction of clauses.
 * Example: (x1 v x2) ^ (~x1 v x3)
 */
void printFormula(const std::vector<Clause>& clauses) {
    for (size_t i = 0; i < clauses.size(); ++i) {
        printClause(clauses[i]);
        if (i < clauses.size() - 1) std::cout << " \342\210\247 ";  // Unicode AND symbol
    }
    std::cout << std::endl;
}

/*
 * Print a quantifier block (e.g., "FORALL x1, x2, x3")
 */
void printQuantifierBlock(const QuantifierBlock& block) {
    std::cout << (block.type == Quantifier::FORALL ? "\342\210\200" : "\342\210\203");  // Unicode quantifiers
    for (size_t i = 0; i < block.variables.size(); i++) {
        std::cout << "x" << block.variables[i];
        if (i < block.variables.size() - 1) std::cout << ",";
    }
}

/*
 * Read a QBF formula from a QDIMACS file.
 *
 * QDIMACS FORMAT:
 * ===============
 * c This is a comment
 * p cnf <num_vars> <num_clauses>
 * a 1 2 3 0          <- universal variables (FORALL)
 * e 4 5 6 0          <- existential variables (EXISTS)
 * 1 -2 3 0           <- clause: x1 OR NOT x2 OR x3
 * -1 4 0             <- clause: NOT x1 OR x4
 *
 * Variables are positive integers.
 * Negation is indicated by negative sign.
 * Lines end with 0.
 */
bool readQBF(const std::string& filename, QBFPreprocessor& preprocessor, bool verbose) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file '" << filename << "'" << std::endl;
        return false;
    }

    std::string line;
    int clauseCount = 0;

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        char type;
        iss >> type;

        // Skip comments and problem line
        if (type == 'c' || type == 'p') {
            continue;
        }

        // Quantifier block: 'a' for FORALL, 'e' for EXISTS
        if (type == 'a' || type == 'e') {
            std::vector<int> variables;
            int var;
            while (iss >> var && var != 0) {
                variables.push_back(var);
            }
            preprocessor.addQuantifierBlock(
                type == 'a' ? Quantifier::FORALL : Quantifier::EXISTS,
                variables
            );
            if (verbose) {
                std::cout << "[PARSE] Quantifier block: "
                          << (type == 'a' ? "FORALL" : "EXISTS") << " ";
                for (size_t i = 0; i < variables.size(); i++) {
                    std::cout << "x" << variables[i];
                    if (i < variables.size() - 1) std::cout << ", ";
                }
                std::cout << std::endl;
            }
        }
        // Clause (starts with a number)
        else if (type == '-' || (type >= '1' && type <= '9')) {
            // Put the first character back and parse the whole line
            std::istringstream clauseStream(line);
            std::vector<Literal> clause;
            int var;
            while (clauseStream >> var && var != 0) {
                bool isNegated = var < 0;
                clause.push_back(Literal(std::abs(var), isNegated));
            }
            if (!clause.empty()) {
                preprocessor.addClause(clause);
                clauseCount++;
            }
        }
    }

    file.close();

    if (verbose) {
        std::cout << "[PARSE] Read " << clauseCount << " clauses" << std::endl;
    }

    return true;
}

/*
 * Print usage information.
 */
void printUsage(const char* programName) {
    std::cout << "QBF Solver - Educational Implementation" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: " << programName << " [-v] <formula.qdimacs>" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -v    Verbose mode - show step-by-step solving trace" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << programName << " formula.qdimacs       # Solve quietly" << std::endl;
    std::cout << "  " << programName << " -v formula.qdimacs    # Solve with trace" << std::endl;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    bool verbose = false;
    std::string filename;

    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg[0] != '-') {
            filename = arg;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    if (filename.empty()) {
        std::cerr << "Error: No input file specified" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    // Read the formula
    QBFPreprocessor preprocessor;
    if (!readQBF(filename, preprocessor, verbose)) {
        return 1;
    }

    // Print the formula
    if (verbose) {
        std::cout << std::endl << "[FORMULA] ";
        const auto& blocks = preprocessor.getQuantifierBlocks();
        for (const auto& block : blocks) {
            printQuantifierBlock(block);
            std::cout << " ";
        }
        printFormula(preprocessor.getClauses());
        std::cout << std::endl;
    }

    // Preprocess (unit propagation, pure literal elimination)
    if (verbose) {
        std::cout << "[PREPROCESS] Running unit propagation and pure literal elimination..." << std::endl;
    }
    preprocessor.preprocess();

    if (verbose) {
        std::cout << "[PREPROCESS] After preprocessing: " << preprocessor.getClauses().size()
                  << " clauses remain" << std::endl;

        // Show any assignments made during preprocessing
        const auto& preAssignments = preprocessor.getAssignments();
        if (!preAssignments.empty()) {
            std::cout << "[PREPROCESS] Determined: ";
            for (const auto& [var, val] : preAssignments) {
                std::cout << "x" << var << "=" << (val ? "true" : "false") << " ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }

    // Solve
    QBFSolver solver;
    solver.setVerbose(verbose);
    Result result = solver.solve(preprocessor);

    // Print result
    std::cout << std::endl;
    if (result == Result::SAT) {
        std::cout << "SATISFIABLE" << std::endl;
        if (verbose) {
            std::cout << std::endl << "The EXISTS player has a winning strategy." << std::endl;
        }
    } else {
        std::cout << "UNSATISFIABLE" << std::endl;
        if (verbose) {
            std::cout << std::endl << "The FORALL player can always falsify the formula." << std::endl;
        }
    }

    return (result == Result::SAT) ? 0 : 1;
}
