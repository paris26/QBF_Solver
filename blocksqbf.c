/*
 BlocksQBF, a generator for random quantified boolean formulae (QBF)
 based on the model described in: 
 "Hubie Chen, Yannet Interian: A Model for Generating Random
 Quantified Boolean Formulas. IJCAI 2005: 66-71".

 Copyright 2010 Florian Lonsing, Johannes Kepler University, Linz, Austria.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or (at
 your option) any later version.

 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define VERSION								\
  "BlocksQBF 1.0\n"							\
  "Copyright 2010 Florian Lonsing, Johannes Kepler University, Linz, Austria.\n"\
  "This is free software; see COPYING for copying conditions.\n"	\
  "There is NO WARRANTY, to the extent permitted by law.\n"


#define USAGE								\
  "usage: blocksqbf <options> <modelparams>\n"				\
  "\n"									\
  "  where <options> is:\n"						\
  "    -h, --help    print usage\n"					\
  "     --version    print version\n"					\
  "            -v    increase verbosity by each '-v'\n"			\
  "     -s 'uint'    random seed (default: start_time * getpid())\n"	\
  "        --sort    sort clauses by variable IDs (default: disabled)\n" \
  "     -d 'uint'    limit for fixing duplicate clauses (default: 100)\n" \
  "\n"									\
  "  where <modelparams is:>\n"						\
  "     -c 'uint'    number of clauses\n"				\
  "     -b 'uint'    number of blocks (innermost block always existential)\n" \
  "    -bc 'uint'    literals in each clause from current block (see example below)\n"	\
  "    -bs 'uint'    size of current block (see example below)\n"	\
  "\n"\
  "Notes:\n"\
  "  - '-bs', '-bc' are incremental: Nth occurrence refers to Nth block etc.\n"	\
  "  - For N blocks, there must be exactly N times '-bc' and N times '-bs'.\n"\
  "  - block size by '-bs' must not be larger than corresponding '-bc'\n"	\
  "\n"\
  "Example: the call 'blocksqbf -c 160 -b 3 -bs 15 -bs 10 -bs 25 -bc 2 -bc 2 -bc 1'\n"\
  "         generates a QBF with 160 clauses, 3 blocks of the form 'eae', block \n" \
  "         sizes of 15 in the first (i.e. leftmost) block, 10 and 25 in the next\n" \
  "         two. Each clause contains exactly 2 literals from the first, 2 from \n" \
  "         the second and 1 from the third block."			\
  "\n\n"


/* Sort generated clauses by literals. */
#define SORT_CLAUSES 0
/* Abort if it was not possible to generate a new clause after
   'DUP_RESOLVE_LIMIT' tries. */
#define DEFAULT_DUP_RESOLVE_LIMIT 100

/* Get random value in range [low,high]. */
#define GET_RAND(low,high) ((rand() % (high-low+1))+low)

/* Prime numbers for hashing clauses. */
static unsigned int primes[] =
  { 1000003, 1000033, 1000037, 1000039, 1000081, 1000099 };
static unsigned int num_primes = sizeof (primes) / sizeof (primes[0]);

typedef struct Clause Clause;

struct Clause
{
  Clause *hash_next;
  int lits[];
};

static unsigned int seed;
static FILE *out;
static unsigned int num_blocks;
static unsigned int num_clauses;
static unsigned int *block_sizes = 0;
static unsigned int num_vars;
static unsigned int *varmarks = 0;
static unsigned int *perblock_nums = 0;
static unsigned int clause_len;
static unsigned int *minblockids = 0;
static unsigned int *maxblockids = 0;
static Clause **clause_table = 0;
static unsigned int dup_resolve_tries;
static unsigned int dup_resolve_limit;
static unsigned int sort_clauses;
static unsigned int verbosity;
static unsigned int done = 0;
static time_t start_time;


static unsigned int
hash (Clause * clause)
{
  unsigned int result = 0, i = 0;

  int *p, *e;
  for (p = clause->lits, e = p + clause_len; p < e; p++)
    {
      result += (*p) * primes[i++];
      if (i == num_primes)
	i = 0;
    }

  return result;
}


/* Compare clauses by sequence of literals. Assumes that the two
   clauses have the same length. */
static int
compare_clauses (Clause * c1, Clause * c2)
{
  int *p1, *p2, *e1, *e2;
  for (p1 = c1->lits, e1 = p1 + clause_len, p2 = c2->lits, e2 =
       p2 + clause_len; p1 < e1; p1++, p2++)
    {
      assert (p2 < e2);
      if (*p1 != *p2)
	return 1;
    }
  return 0;
}


static Clause **
find_clause (Clause * clause)
{
  Clause **pp, *p;

  pp = clause_table + (hash (clause) % num_clauses);
  for (; (p = *pp) && compare_clauses (p, clause); pp = &(p->hash_next))
    ;

  return pp;
}


static Clause *
create_clause (unsigned int num_lits)
{
  size_t bytes = sizeof (Clause) + num_lits * sizeof (int);
  Clause *clause = malloc (bytes);
  memset (clause, 0, bytes);
  return clause;
}


static void
delete_clause (Clause * clause)
{
  free (clause);
}


static void
print_usage ()
{
  fprintf (stderr, "%s", USAGE);
}


static int
isposintstr (char *str)
{
  int found_nonzero_digit = 0;
  /* Empty string is not considered as number-string. */
  if (!*str)
    return 0;
  char *p;
  for (p = str; *p; p++)
    {
      char c = *p;
      if (!isdigit (c))
	return 0;
      found_nonzero_digit = found_nonzero_digit || (c != '0');
    }
  if (!found_nonzero_digit)
    return 0;
  else
    return 1;
}


static int
iszeroposintstr (char *str)
{
  /* Empty string is not considered as number-string. */
  if (!*str)
    return 0;
  char *p;
  for (p = str; *p; p++)
    {
      char c = *p;
      if (!isdigit (c))
	return 0;
    }
  return 1;
}


static void
set_default_options ()
{
  num_blocks = 2;
  num_clauses = 100;

  /* Initialize block sizes. */
  block_sizes = malloc (num_blocks * sizeof (unsigned int));
  block_sizes[0] = 10;
  block_sizes[1] = 60;

  /* Set number of variables taken from each block. */
  perblock_nums = malloc (num_blocks * sizeof (unsigned int));
  perblock_nums[0] = 1;
  perblock_nums[1] = 2;
}


static void
print_config (int argc, char **argv, FILE * file, int prefix_c)
{
  assert (argc > 0);
  unsigned int i;
  if (!prefix_c)
    {
      fprintf (file, "qbfgen params:");
      for (i = 0; i < (unsigned int) argc; i++)
	fprintf (file, " %s", argv[i]);
      fprintf (file, "\n");
      fprintf (file, "time: %s", asctime (localtime (&start_time)));
      fprintf (file, "seed = %u\n", seed);
      fprintf (file, "sort clauses = %s\n", sort_clauses ? "yes" : "no");
      fprintf (file, "dup. resolve limit = %d\n", dup_resolve_limit);
      fprintf (file, "verbosity = %d\n", verbosity);
      fprintf (file, "num blocks = %d\n", num_blocks);
      fprintf (file, "num clauses = %d\n", num_clauses);
      for (i = 0; i < num_blocks; i++)
	fprintf (file, "block_sizes[%d] = %d\n", i, block_sizes[i]);
      fprintf (file, "num vars = %d\n", num_vars);
      for (i = 0; i < num_blocks; i++)
	fprintf (file, "perblock_nums[%d] = %d\n", i, perblock_nums[i]);
      fprintf (file, "clause len = %d\n", clause_len);
      for (i = 0; i < num_blocks; i++)
	{
	  fprintf (file, "minblockids[%d] = %d\n", i, minblockids[i]);
	  fprintf (file, "maxblockids[%d] = %d\n", i, maxblockids[i]);
	}
    }
  else
    {
      fprintf (file, "c qbfgen params:");
      for (i = 0; i < (unsigned int) argc; i++)
	fprintf (file, " %s", argv[i]);
      fprintf (file, "\n");
      fprintf (file, "c time: %s", asctime (localtime (&start_time)));
      fprintf (file, "c seed = %u\n", seed);
      fprintf (file, "c sort clauses = %s\n", sort_clauses ? "yes" : "no");
      fprintf (file, "c dup. resolve limit = %d\n", dup_resolve_limit);
      fprintf (file, "c verbosity = %d\n", verbosity);
      fprintf (file, "c num blocks = %d\n", num_blocks);
      fprintf (file, "c num clauses = %d\n", num_clauses);
      for (i = 0; i < num_blocks; i++)
	fprintf (file, "c block_sizes[%d] = %d\n", i, block_sizes[i]);
      fprintf (file, "c num vars = %d\n", num_vars);
      for (i = 0; i < num_blocks; i++)
	fprintf (file, "c perblock_nums[%d] = %d\n", i, perblock_nums[i]);
      fprintf (file, "c clause len = %d\n", clause_len);
      for (i = 0; i < num_blocks; i++)
	{
	  fprintf (file, "c minblockids[%d] = %d\n", i, minblockids[i]);
	  fprintf (file, "c maxblockids[%d] = %d\n", i, maxblockids[i]);
	}
    }
}


static void
print_clause (FILE * file, Clause * clause)
{
  int *p, *e;
  for (p = clause->lits, e = p + clause_len; p < e; p++)
    {
      assert (*p);
      fprintf (file, "%d ", *p);
    }
  fprintf (file, "0\n");
}


static int
qsort_compare_lits (const void *litp1, const void *litp2)
{
  int lit1 = *((int *) litp1);
  assert (lit1);
  unsigned int var1 = lit1 < 0 ? -lit1 : lit1;
  int lit2 = *((int *) litp2);
  assert (lit2);
  unsigned int var2 = lit2 < 0 ? -lit2 : lit2;
  if (var1 < var2)
    return -1;
  else if (var1 > var2)
    return 1;
  else
    return 0;
}


static void
sort_clause (Clause * clause)
{
  qsort (clause->lits, clause_len, sizeof (int), qsort_compare_lits);
}


/* Parse options. Returns non-zero in case of failure. */
static int
parse_args_and_setup (int argc, char **argv)
{
  int b_specified = 0;
  int c_specified = 0;
  unsigned int bc_occurred_cnt = 0;
  unsigned int bs_occurred_cnt = 0;
  int i;
  for (i = 1; i < argc; i++)
    {
      char *argstr = argv[i];
      if (!strcmp (argstr, "-h") || !strcmp (argstr, "--help"))
	{
	  print_usage ();
	  done = 1;
	  return 0;
	}
      else if (!strcmp (argstr, "--version"))
	{
	  fprintf (stderr, "%s\n", VERSION);
	  done = 1;
	  return 0;
	}
      else if (!strcmp (argstr, "--sort"))
	{
	  sort_clauses = 1;
	}
      else if (!strcmp (argstr, "-v"))
	{
	  verbosity++;
	}
      else if (!strcmp (argstr, "-d"))
	{
	  /* Limit for duplication resolution. */
	  if (++i >= argc || !isposintstr (argstr = argv[i]))
	    {
	      fprintf (stderr, "Expecting positive integer after '-d'!\n");
	      return 1;
	    }
	  dup_resolve_limit = atoi (argstr);
	}
      else if (!strcmp (argstr, "-c"))
	{
	  /* Number of clauses. */
	  if (++i >= argc || !isposintstr (argstr = argv[i]))
	    {
	      fprintf (stderr, "Expecting positive integer after '-c'!\n");
	      return 1;
	    }
	  num_clauses = atoi (argstr);
	  c_specified = 1;
	}
      else if (!strcmp (argstr, "-s"))
	{
	  /* Random seed. */
	  if (++i >= argc || !iszeroposintstr (argstr = argv[i]))
	    {
	      fprintf (stderr,
		       "Expecting non-negative integer after '-s'!\n");
	      return 1;
	    }
	  seed = atoi (argstr);
	}
      else if (!strcmp (argstr, "-b"))
	{
	  /* Number of blocks. */
	  if (b_specified)
	    {
	      fprintf (stderr, "Must not have '-b' multiple times!\n");
	      return 1;
	    }
	  if (++i >= argc || !isposintstr (argstr = argv[i]))
	    {
	      fprintf (stderr, "Expecting positive integer after '-b'!\n");
	      return 1;
	    }
	  num_blocks = atoi (argstr);
	  b_specified = 1;
	  /* Set up data structures. */
	  block_sizes = malloc (num_blocks * sizeof (unsigned int));
	  perblock_nums = malloc (num_blocks * sizeof (unsigned int));
	}
      else if (!strcmp (argstr, "-bc"))
	{
	  /* Number of literals from a block incrementally (outside-in). */
	  if (!b_specified)
	    {
	      fprintf (stderr, "Expecting '-b' before '-bc'!\n");
	      return 1;
	    }
	  if (++i >= argc || !isposintstr (argstr = argv[i]))
	    {
	      fprintf (stderr, "Expecting positive integer after '-bc'!\n");
	      return 1;
	    }
	  if (bc_occurred_cnt == num_blocks)
	    {
	      fprintf (stderr, "Two many occurrences of '-bc'!\n");
	      return 1;
	    }
	  unsigned int cur_bc = atoi (argstr);
	  perblock_nums[bc_occurred_cnt++] = cur_bc;
	}
      else if (!strcmp (argstr, "-bs"))
	{
	  /* Number of variables in a block incrementally (outside-in). */
	  if (!b_specified)
	    {
	      fprintf (stderr, "Expecting '-b' before '-bs'!\n");
	      return 1;
	    }
	  if (++i >= argc || !isposintstr (argstr = argv[i]))
	    {
	      fprintf (stderr, "Expecting positive integer after '-bs'!\n");
	      return 1;
	    }
	  if (bs_occurred_cnt == num_blocks)
	    {
	      fprintf (stderr, "Two many occurrences of '-bs'!\n");
	      return 1;
	    }
	  unsigned int cur_bs = atoi (argstr);
	  block_sizes[bs_occurred_cnt++] = cur_bs;
	}
      else
	{
	  fprintf (stderr, "Unknown argument %s\n", argstr);
	  return 1;
	}
    }

  if (!c_specified)
    {
      fprintf (stderr, "Expecting number of clauses by '-c'!\n");
      return 1;
    }
  if (!b_specified)
    {
      fprintf (stderr, "Expecting number of quantifier blocks by '-b'!\n");
      return 1;
    }
  if (bc_occurred_cnt != num_blocks)
    {
      fprintf (stderr, "Expecting '-bc' for each quantifier block!\n");
      return 1;
    }
  if (bs_occurred_cnt != num_blocks)
    {
      fprintf (stderr, "Expecting '-bs' for each quantifier block!\n");
      return 1;
    }
  for (i = 0; i < (int) num_blocks; i++)
    {
      if (perblock_nums[i] > block_sizes[i])
	{
	  fprintf (stderr, "Num. of literals taken from "
		   "block must not be greater than block size!\n");
	  return 1;
	}
    }

  return 0;
}


static void
clean_up ()
{
  if (clause_table)
    free (clause_table);
  if (varmarks)
    free (varmarks);
  if (block_sizes)
    free (block_sizes);
  if (perblock_nums)
    free (perblock_nums);
  if (minblockids)
    free (minblockids);
  if (maxblockids)
    free (maxblockids);
}


int
main (int argc, char **argv)
{
  unsigned int i;
  sort_clauses = SORT_CLAUSES;
  dup_resolve_limit = DEFAULT_DUP_RESOLVE_LIMIT;
  verbosity = 0;
  out = stdout;
  start_time = time (NULL);
  seed = start_time * getpid ();

  if (argc == 1)
    set_default_options ();
  else if (parse_args_and_setup (argc, argv))
    {
      clean_up ();
      exit (EXIT_FAILURE);
    }
  else if (done)
    {
      clean_up ();
      exit (EXIT_SUCCESS);
    }

  clause_table = malloc (num_clauses * sizeof (Clause *));
  memset (clause_table, 0, num_clauses * sizeof (Clause *));

  /* Count total variables. */
  num_vars = 0;
  for (i = 0; i < num_blocks; i++)
    num_vars += block_sizes[i];

  /* Mark table for variables in clauses. */
  varmarks = malloc (num_vars * sizeof (unsigned int));
  memset (varmarks, 0, num_vars * sizeof (unsigned int));

  clause_len = 0;
  /* Get clause length. */
  for (i = 0; i < num_blocks; i++)
    clause_len += perblock_nums[i];

  /* Min. and max. variable ID in each block. */
  minblockids = malloc (num_blocks * sizeof (unsigned int));
  maxblockids = malloc (num_blocks * sizeof (unsigned int));
  minblockids[0] = 1;
  maxblockids[0] = block_sizes[0];
  for (i = 1; i < num_blocks; i++)
    {
      unsigned int minid = maxblockids[i - 1] + 1;
      unsigned int maxid = minid + block_sizes[i] - 1;
      minblockids[i] = minid;
      maxblockids[i] = maxid;
    }

  srand (seed);
  print_config (argc, argv, out, 1);

  /* Print preamble. */
  fprintf (out, "p cnf %d %d\n", num_vars, num_clauses);

  /* Print quantifier blocks. */
  char type = (num_blocks % 2) ? 'e' : 'a';
  for (i = 0; i < num_blocks; i++)
    {
      fprintf (out, "%c ", type);
      unsigned int id;
      for (id = minblockids[i]; id <= maxblockids[i]; id++)
	fprintf (out, "%d ", id);
      fprintf (out, "0\n");
      type = type == 'e' ? 'a' : 'e';
    }

  Clause *clause = create_clause (clause_len);
  dup_resolve_tries = 0;

  /* Generate clauses. */
  unsigned curclause;
  for (curclause = 0; curclause < num_clauses; curclause++)
    {
      /* Reset marks for literals in clauses. */
      memset (varmarks, 0, num_vars * sizeof (unsigned int));
      int *litpos = clause->lits;

      /* For all blocks... */
      unsigned int block;
      for (block = 0; block < num_blocks; block++)
	{
	  /* ...add specified number of literals at random. */
	  unsigned int blocklitcnt;
	  for (blocklitcnt = 0; blocklitcnt < perblock_nums[block];
	       blocklitcnt++)
	    {
	      assert (clause->lits <= litpos);
	      assert (litpos < clause->lits + clause_len);
	      unsigned int var =
		GET_RAND (minblockids[block], maxblockids[block]);
	      assert (1 <= var && var <= num_vars);

	      if (varmarks[var - 1])
		{
		  /* Literal of 'var' already in clause. */
		  assert (blocklitcnt > 0);
		  blocklitcnt--;
		  continue;
		}
	      else
		varmarks[var - 1] = 1;

	      /* Negate literal at random. */
	      int lit = var;
	      if (GET_RAND (0, 1))
		lit = -lit;

	      *litpos++ = lit;
	    }
	}

      if (sort_clauses)
	sort_clause (clause);

      if (verbosity >= 1)
	{
	  fprintf (stderr, "generated clause: ");
	  print_clause (stderr, clause);
	}

      Clause **p = find_clause (clause);
      if (!(*p))
	{
	  /* Insert clause. */
	  *p = clause;
	  clause = create_clause (clause_len);
	  dup_resolve_tries = 0;
	}
      else
	{
	  /* Clause already generated -> try again. */
	  if (dup_resolve_tries == dup_resolve_limit)
	    {
	      if (verbosity >= 1)
		fprintf (stderr,
			 "Aborting after %d tries to resolve duplicate clause.\n",
			 dup_resolve_tries);
	      break;
	    }
	  assert (curclause > 0);
	  curclause--;
	  if (verbosity >= 1)
	    {
	      fprintf (stderr, "skipping duplicate clause (%d tries): ",
		       dup_resolve_tries);
	      print_clause (stderr, clause);
	    }
	  dup_resolve_tries++;
	  continue;
	}
    }

  /* Delete clause allocated at end of last loop iteration. */
  delete_clause (clause);

  /* Print and free clauses. */
  Clause *next;
  for (i = 0; i < num_clauses; i++)
    for (clause = clause_table[i]; clause; clause = next)
      {
	next = clause->hash_next;
	print_clause (out, clause);
	delete_clause (clause);
      }

  clean_up ();
  exit (EXIT_SUCCESS);
}
