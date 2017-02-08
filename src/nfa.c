#include "../include/nfa.h"

/// This is an implementation of Thompson's Construction to obtain NFAs
/// from regexs. For more details check "Engineering a Compiler", 2011,
/// Section 2.4.2

// Since for each regex (or a combination of 2 regexs), we need a constant
// number of new epsilon transitions (specifically, 1 for concatination
// , 4 for or, and 4 for Kleene), the maximum total number in the final
// NFA will be c * (MAX_NESTED_EXPRS + MAX_NONTERMS) for some constant
// c < 5 (see regex.h for a definition of MAX_NESTED_EXPRS and MAX_NONTERMS).
// I use a constant factor of 10 here because in regex spec a reserved
// word is represented as a single unit while in the NFAs, reserved words
// are split to their individual characters. This gives an average of 6
// characters per reserved word, assuming that all terminals were reserved
// words which is too generous and more than enough.
#define MAX_NFA_EDGES      10 * (MAX_NESTED_EXPRS + MAX_NONTERMS)
#define MAX_EDGES_PER_NODE 128
#define MAX_NFA_STATES     1024
#define MAX_NFAS           MAX_NFA_STATES / 4
#define EPSILON            0
#define DEBUG              1

struct NFAEdge;

typedef enum {
  START,
  INTERNAL,
  ACCEPTING
} NFAStateType;

typedef struct NFAState {
  PoolOffset edges[MAX_EDGES_PER_NODE];
  int numEdges;
  NFAStateType type;
#if DEBUG
  bool visited;
#endif
} NFAState, *NFAStatePtr;

typedef struct NFAEdge {
  PoolOffset target;
  char symbol;
} NFAEdge, *NFAEdgePtr;

typedef struct NFA {
  PoolOffset start;
  PoolOffset accepting;
} NFA, *NFAPtr;

static NFAState nfaStatesPool[MAX_NFA_STATES];
static PoolOffset currentNFAState = 0;

static NFAEdge nfaEdgePool[MAX_NFA_EDGES];
static PoolOffset currentNFAEdge = 0;

static NFA nfaPool[MAX_NFAS];
static PoolOffset currentNFA = 0;

static PoolOffset new_start_state();
static PoolOffset new_state(NFAStateType type);
static PoolOffset new_accepting_state();
static PoolOffset new_edge(PoolOffset target, char symbol);
static PoolOffset new_nfa();
static PoolOffset build_single_symbol_nfa(char symbol);
static void concat_nfa(PoolOffset nfa1Idx, PoolOffset nfa2Idx);

#if DEBUG
static void print_nfa(PoolOffset nfaIdx);
static void print_state(PoolOffset stateIdx);
#endif

void build_nfa(NonTerminalPtr nontermTable, int nontermTableSize) {
  PoolOffset a = build_single_symbol_nfa('a');
  PoolOffset b = build_single_symbol_nfa('b');
  PoolOffset c = build_single_symbol_nfa('c');
  concat_nfa(a, b);
  concat_nfa(c, a);
  print_nfa(c);
}

/// Concatinates nfa2 to nfa1
///
///         nfa1      INPUTS      nfa2
///    ---  sym   ===        ---  sym   ===
///   | a | ---> | b |      | c | ---> | d |
///    ---        ===        ---        ===
///
///                   OUTPUT
///                    nfa1
///    ---  sym   ---  eps   ---  sym   ===
///   | a | ---> | b | ---> | c | ---> | d |
///    ---        ---        ---        ===
// TODO nfa2 becomes unsed storage after this, reuse that memory
// Check: https://github.com/KareemErgawy/al-farahidi/issues/2
static void concat_nfa(PoolOffset nfa1Idx, PoolOffset nfa2Idx) {
  assert(nfa1Idx != nfa2Idx && "Trying to concat an NFA to itself!\n");
  NFAPtr nfa1 = nfaPool + nfa1Idx;
  NFAPtr nfa2 = nfaPool + nfa2Idx;
  NFAStatePtr nfa1Accepting = nfaStatesPool + nfa1->accepting;
  nfa1Accepting->type = INTERNAL;
  nfa1Accepting->edges[nfa1Accepting->numEdges] = new_edge(nfa2->start,
                                                           (char)EPSILON);
  nfa1Accepting->numEdges++;
  nfa1->accepting = nfa2->accepting;
}

/// Build the NFA for a single symbol in the alphabet
///
///        OUTPUT
///    ---  sym   ===
///   | a | ---> | b |
///    ---        === 
static PoolOffset build_single_symbol_nfa(char symbol) {
  PoolOffset nfaIdx = new_nfa();
  NFA nfa = nfaPool[nfaIdx];
  nfaStatesPool[nfa.start].edges[0] = new_edge(nfa.accepting, symbol);
  nfaStatesPool[nfa.start].numEdges++;
  return nfaIdx;
}

/// Gets a free state from the pool and returns its index
static PoolOffset new_start_state() {
  return new_state(START);
}

/// Gets a free state from the pool and returns its index
static PoolOffset new_accepting_state() {
  return new_state(ACCEPTING);
}

static PoolOffset new_state(NFAStateType type) {
  assert(currentNFAState < MAX_NFA_STATES && "NFA states pool ran out of"
         "memory!\n");
  nfaStatesPool[currentNFAState].type = type;
  nfaStatesPool[currentNFAState].numEdges = 0;
  return currentNFAState++;
}

static PoolOffset new_edge(PoolOffset target, char symbol) {
  assert(currentNFAEdge < MAX_NFA_EDGES && "NFA edges pool ran out of"
         "memory!\n");
  nfaEdgePool[currentNFAEdge].target = target;
  nfaEdgePool[currentNFAEdge].symbol = symbol;
  return currentNFAEdge++;
}

static PoolOffset new_nfa() {
  assert(currentNFA < MAX_NFAS && "NFA pool ran out of memory!\n");
  nfaPool[currentNFA].start = new_start_state();
  nfaPool[currentNFA].accepting = new_accepting_state();
  return currentNFA++;
}

#if DEBUG
static void print_nfa(PoolOffset nfaIdx) {
  print_state(nfaPool[nfaIdx].start);
}

static void print_state(PoolOffset stateIdx) {
  NFAState state = nfaStatesPool[stateIdx];

  if (state.visited) {
    return;
  }

  state.visited = TRUE;
  log("State %d ", stateIdx);

  switch(state.type) {
  case START:
    log("<start>");
    break;
  case ACCEPTING:
    log("<accept>");
    break;
  case INTERNAL:
    break;
  }

  log("\n");

  for (int i=0 ; i<state.numEdges ; i++) {
    NFAEdge edge = nfaEdgePool[state.edges[i]];
    log ("\t==(Symbol %c)==> State %d\n", edge.symbol, edge.target);
  }

  for (int i=0 ; i<state.numEdges ; i++) {
    NFAEdge edge = nfaEdgePool[state.edges[i]];
    print_state(edge.target);
  }

  state.visited = FALSE;
}
#endif
