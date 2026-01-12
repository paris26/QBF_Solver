# QBF Solver - Educational Implementation

An educational implementation of a **Quantified Boolean Formula (QBF)** solver in C++. This project is designed to help programmers understand QBF solving algorithms through clear code, thorough comments, and step-by-step tracing.

## What is QBF?

**Quantified Boolean Formula (QBF)** extends regular Boolean satisfiability (SAT) by adding quantifiers: **"for all" (∀)** and **"there exists" (∃)**.

### Regular SAT vs QBF

**SAT** asks: "Is there an assignment that makes this formula true?"
```
Example: (x ∨ y) ∧ (¬x ∨ z)
Question: Can we find values for x, y, z that satisfy this?
Answer: Yes! x=false, y=true, z=anything works.
```

**QBF** adds quantifiers that change the question:
```
Example: ∀x ∃y (x ∨ y) ∧ (¬x ∨ ¬y)
Question: For ALL values of x, can we find SOME y that works?
Answer: Yes!
  - If x=true:  set y=false → (T∨F)∧(F∨T) = T∧T = T ✓
  - If x=false: set y=true  → (F∨T)∧(T∨F) = T∧T = T ✓
```

### The Game Interpretation

Think of QBF as a **two-player game**:

- **∃ (EXISTS) player**: Tries to make the formula TRUE
- **∀ (FORALL) player**: Tries to make the formula FALSE

The formula is **satisfiable** if the EXISTS player has a **winning strategy** - a way to respond to any FORALL move and still satisfy the formula.

```
Formula: ∀x ∃y (x ∨ y) ∧ (¬x ∨ ¬y)

Game tree:
                    [start]
                       |
              FORALL picks x
                  /        \
            x=true        x=false
               |              |
         EXISTS picks y   EXISTS picks y
           /     \          /      \
      y=true  y=false  y=true  y=false
         |        |        |        |
       FALSE    TRUE     TRUE    FALSE
                  ↑        ↑
            EXISTS wins  EXISTS wins

EXISTS can always win by responding:
- x=true  → pick y=false
- x=false → pick y=true

Therefore: SATISFIABLE
```

### Why is QBF Important?

1. **Complexity**: QBF is **PSPACE-complete** (harder than NP-complete SAT)
2. **Applications**:
   - Formal verification (does a circuit work for ALL inputs?)
   - AI planning (can we win regardless of opponent's moves?)
   - Game solving (chess, go sub-problems)
   - Automated reasoning

## How This Solver Works

### Algorithm Overview

We use a **DPLL-based recursive search** adapted for QBF:

```
solve(formula):
    // Base cases
    if formula has empty clause: return UNSAT
    if all clauses satisfied: return SAT

    // Pick next variable (following quantifier order)
    x = next unassigned variable

    if x is EXISTENTIAL:
        // EXISTS wins if EITHER branch works
        if solve(formula[x=true]) == SAT: return SAT
        if solve(formula[x=false]) == SAT: return SAT
        return UNSAT

    if x is UNIVERSAL:
        // EXISTS needs BOTH branches to work
        if solve(formula[x=true]) == UNSAT: return UNSAT
        if solve(formula[x=false]) == UNSAT: return UNSAT
        return SAT
```

### Key Insight: Different Branching Semantics

| Variable Type | Condition for SAT | Analogy |
|--------------|-------------------|---------|
| ∃ (EXISTS) | ONE branch succeeds | OR: try until you find one that works |
| ∀ (FORALL) | BOTH branches succeed | AND: must verify all possibilities |

### Preprocessing

Before searching, we simplify using:

1. **Unit Propagation**: If a clause has one literal, it must be true
2. **Pure Literal Elimination**: Variables appearing in only one polarity can be safely assigned

## QDIMACS Format

Input files use the standard **QDIMACS** format:

```
c This is a comment
c Formula: ∀x1,x2 ∃y1,y2 (x1 ∨ y1) ∧ (¬x2 ∨ y2)
p cnf 4 2
a 1 2 0
e 3 4 0
1 3 0
-2 4 0
```

### Format Breakdown

| Line | Meaning |
|------|---------|
| `c ...` | Comment (ignored) |
| `p cnf 4 2` | Problem line: 4 variables, 2 clauses |
| `a 1 2 0` | Universal (∀) block: variables 1 and 2 |
| `e 3 4 0` | Existential (∃) block: variables 3 and 4 |
| `1 3 0` | Clause: (x1 ∨ x3) |
| `-2 4 0` | Clause: (¬x2 ∨ x4) |

**Rules:**
- Variables are positive integers (1, 2, 3, ...)
- Negative number = negated variable (-1 means ¬x1)
- Lines end with 0
- `a` = FORALL, `e` = EXISTS

## Usage

### Building

```bash
make        # Build the solver
make debug  # Build with debug symbols
```

### Running

```bash
./qbf formula.qdimacs           # Solve (quiet mode)
./qbf -v formula.qdimacs        # Solve with step-by-step trace
./qbf --help                    # Show help
```

### Example with Verbose Output

```bash
$ ./qbf -v test/trivial_sat.qdimacs

[PARSE] Quantifier block: EXISTS x1
[PARSE] Read 1 clauses

[FORMULA] ∃x1 (x1)

[PREPROCESS] Running unit propagation and pure literal elimination...
[PREPROCESS] After preprocessing: 1 clauses remain

[SOLVE] Starting with 1 clauses, 1 quantifier blocks
  [DECIDE] x1 = true (EXISTS)
    [SUCCESS] All clauses satisfied

SATISFIABLE

The EXISTS player has a winning strategy.
```

## Project Structure

```
QBF_Solver/
├── README.md              # This file
├── Makefile               # Build configuration
├── main.cpp               # Entry point, CLI, parsing
├── QBFPreprocessor.h      # Data structures & preprocessing
├── QBFPreprocessor.cpp    # Preprocessing implementation
├── QBFSolver.h            # Solver interface
├── QBFSolver.cpp          # DPLL-QBF algorithm
├── formula.txt            # Example formula
├── test/                  # Test cases
│   ├── trivial_sat.qdimacs
│   ├── trivial_unsat.qdimacs
│   └── ...
└── blocksqbf.c            # Random formula generator
```

## Code Walkthrough

### Data Structures (`QBFPreprocessor.h`)

```cpp
// A literal is a possibly-negated variable
struct Literal {
    int variable;     // e.g., 3
    bool isNegated;   // true = ¬x3, false = x3
};

// Quantifier blocks group variables
struct QuantifierBlock {
    Quantifier type;           // FORALL or EXISTS
    vector<int> variables;     // Variables in this block
};

// A clause is a disjunction (OR) of literals
using Clause = vector<Literal>;  // e.g., (x1 ∨ ¬x2 ∨ x3)
```

### Solver Core (`QBFSolver.cpp`)

The key function is `solve_recursive()`:

```cpp
Result QBFSolver::solve_recursive() {
    // Base cases
    if (hasEmptyClause()) return Result::UNSAT;
    if (allClausesSatisfied()) return Result::SAT;

    int var = findNextUnassignedVar();
    Quantifier qtype = varToQuantifier[var];

    if (qtype == Quantifier::EXISTS) {
        // Try to find ONE working value
        if (tryValue(var, true) == SAT) return SAT;
        if (tryValue(var, false) == SAT) return SAT;
        return UNSAT;
    } else {  // FORALL
        // Need BOTH values to work
        if (tryValue(var, true) == UNSAT) return UNSAT;
        if (tryValue(var, false) == UNSAT) return UNSAT;
        return SAT;
    }
}
```

## Test Cases

| File | Expected | What it Tests |
|------|----------|---------------|
| `trivial_sat.qdimacs` | SAT | Simplest satisfiable formula |
| `trivial_unsat.qdimacs` | UNSAT | Simplest unsatisfiable formula |
| `forall_both.qdimacs` | SAT | FORALL requires both branches |
| `exists_one.qdimacs` | SAT | EXISTS needs only one branch |

Run all tests:
```bash
make test
```

## Learning Resources

### Understanding the Code

1. Start with `main.cpp` to see the overall flow
2. Read `QBFPreprocessor.h` for data structures
3. Study `QBFSolver.cpp` for the algorithm (well-commented)
4. Run with `-v` to see the algorithm in action

### Further Reading

- [QBF in Computational Complexity](https://en.wikipedia.org/wiki/True_quantified_Boolean_formula)
- [DPLL Algorithm](https://en.wikipedia.org/wiki/DPLL_algorithm)
- [QDIMACS Format](http://www.qbflib.org/qdimacs.html)

## Limitations

This is an **educational implementation**, not production-ready:

- No conflict-driven clause learning (CDCL)
- No dependency schemes optimization
- No certificate generation
- Simple variable ordering

For production use, consider [DepQBF](https://github.com/lonsing/depqbf) or [QFUN](https://www.react.uni-saarland.de/tools/qfun/).

## License

MIT License - feel free to use for learning and teaching.
