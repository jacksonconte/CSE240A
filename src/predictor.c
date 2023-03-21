//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "NAME";
const char *studentID = "PID";
const char *email = "EMAIL";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = {"Static", "Gshare",
                         "Tournament", "Custom"};

int ghistoryBits; // Number of bits used for Global History
int lhistoryBits; // Number of bits used for Local History
int pcIndexBits;  // Number of bits used for PC index
int bpType;       // Branch Prediction Type
int verbose;

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

uint8_t *gPredictors;
uint8_t *lPredictors; // local prediction
uint8_t *cPredictors; // choice predictions
uint16_t *lHistory;   // local history table or PHT

/*

p = pc bits, n = weights

each perceptron: [w0...wn]
each weight needs to be signed char from -128 to 127?
n = 8 -> 8 signed chars per perceptron
GHR needs to be n bits for dot product to work

# of perceptrons = 2^p where p = pc bits

2^p * n * 8 bits = 64 kbits
2^p * n = 8 kbits
2^p = 2^13 / n

log 2^p = log(2^13 / n)
p = log(2^13) - log n
p = 13 - log n
*/

// ADJUST PERCEPTRON CONSTANTS HERE
#define P_WEIGHTS 8 // x on graph, also # of ghr bits
#define P_PCBITS 10 // y on graph
#define THRESH 127 // theta threshold

int8_t ptrons[1 << P_PCBITS][P_WEIGHTS];
int y = 0; 

// Threshold values
#define THETA (P_WEIGHTS * 1.93) + 14
uint32_t ghr = 0;

uint32_t numPredictors;
uint8_t last_local = SN;
uint8_t last_global = SN;


//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//
void init_predictor()
{
  printf("========= %s =========\n", bpName[bpType]);
  // initalizes structures for static
  if (bpType == STATIC)
  {
    return;
  }
  // initializes structures for GShare
  if (bpType == GSHARE)
  {
    if (ghistoryBits > 31)
    {
      fprintf(stderr, "ERROR: most bits supported for gshare is 31\n");
      exit(1);
    }
    pcIndexBits = ghistoryBits;
    numPredictors = 1 << ghistoryBits;
    printf("# predictors = %u\n\n", numPredictors);

    gPredictors = malloc(numPredictors / 4); // freed on exit lmao

    // initialize all to strongly not taken
    for (uint32_t i = 0; i < numPredictors / 4; i++)
      gPredictors[i] = SN;
  }
  // initalizes structures for Tournament
  else if (bpType == TOURNAMENT)
  {
    // TODO: check bit #s
    uint32_t numGPredictors = 1 << ghistoryBits;
    gPredictors = malloc(numGPredictors / 4);

    uint32_t numCPredictors = 1 << ghistoryBits;
    cPredictors = malloc(numGPredictors / 4);

    uint32_t numLPredictors = 1 << lhistoryBits;
    lPredictors = malloc(numLPredictors / 4);
    printf("lHistoryBits: %d\n", lhistoryBits);

    // 2^pc index bits
    uint32_t numLHistory = 1 << pcIndexBits;
    lHistory = malloc(numLHistory * 2); // assume max. 16 bits of history for now

    for (uint32_t i = 0; i < numGPredictors / 4; i++)
      gPredictors[i] = SN;
    for (uint32_t i = 0; i < numGPredictors / 4; i++)
      cPredictors[i] = SN;
    // initialize to weakly global
    for (uint32_t i = 0; i < numLPredictors / 4; i++)
      lPredictors[i] = 2;
  }

  // initalizes structures for Custom
  else if (bpType == CUSTOM) {
    pcIndexBits = P_PCBITS;
    ghistoryBits = P_WEIGHTS;

    for (int i = 0; i < (1 << P_PCBITS); i++){
      for (int j = 0; j < P_WEIGHTS; j++) {
        ptrons[0][0] = 0;
      }
    }
    ghr = 0;
    
  }


  else
  {
    fprintf(stderr, "shit's fucked bro\n");
    exit(1);
  }
}


// helper function
uint8_t
get_predictor(uint8_t *array, uint32_t i)
{
  uint8_t fourPredictors = array[i / 4];
  uint8_t ind = i % 4;
  // only return last two bits
  return (fourPredictors >> (2 * ind)) & 0b11;
}
// helper function
// 0 <= state <= 3
void set_predictor(uint8_t *array, uint32_t i, uint8_t state)
{
  uint8_t fourPredictors = array[i / 4];
  uint8_t ind = i % 4;                 // 0-3
  uint8_t mask = ~(0b11 << (2 * ind)); // 2*ind = 4; mask = 11001111
  fourPredictors &= mask;
  fourPredictors |= (state << (2 * ind)); // 11 00 11 11 | 00 01 00 00
  array[i / 4] = fourPredictors;          // it just works(tm)
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t
make_prediction(uint32_t pc)
{
  if (bpType == STATIC)
  {
    return TAKEN;
  }
  else if (bpType == GSHARE)
  {
    // mask by index bits
    uint32_t pcBits = pc & ((1 << pcIndexBits) - 1);
    uint32_t ghrBits = ghr & ((1 << ghistoryBits) - 1);
    uint32_t ind = pcBits ^ ghrBits;
    uint8_t state = get_predictor(gPredictors, ind);

    if (state > 1)
      return TAKEN;
    else
      return NOTTAKEN;
  }
  else if (bpType == TOURNAMENT)
  {
    // mask by index bits
    uint32_t pcBits = pc & ((1 << pcIndexBits) - 1);
    uint32_t ghrBits = ghr & ((1 << ghistoryBits) - 1);

    uint16_t *localHist = &(lHistory[pcBits]);
    uint16_t localPred = *localHist & ((1 << lhistoryBits) - 1);

    last_local = get_predictor(lPredictors, localPred);
    last_global = get_predictor(gPredictors, ghrBits);
    uint8_t choice = get_predictor(cPredictors, ghrBits);

    // local
    if (choice < 2)
    {
      return (last_local > WN); // return 1 if taken
    }
    else
    { // global
      return (last_global > WN);
    }
  }
  else if (bpType == CUSTOM) {
    // select entry in perceptron table
    uint32_t pcBits = pc & ((1 << pcIndexBits) - 1);
    uint32_t ghrBits = ghr & ((1 << ghistoryBits) - 1);
    pcBits ^= ghrBits; 

    y = ptrons[pcBits][0];
    for (int i = 1; i < P_WEIGHTS; i++) {
      if (((ghr >> i)&1) == 1) {
        y += ptrons[pcBits][i]*1;
      } else {
        y += ptrons[pcBits][i]*(-1);
      }
    }
    
    if (y >= 0) {
      return TAKEN;
    } else {
      return NOTTAKEN;
    }
  }
  else
  {
    fprintf(stderr, "damn it\n");
    exit(1);
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//
void train_predictor(uint32_t pc, uint8_t outcome)
{
  if (bpType == STATIC)
  {
    return;
  }
  else if (bpType == GSHARE)
  {

    // Add masking to the pcBits and ghrbits
    uint32_t pcBits = pc & ((1 << pcIndexBits) - 1);
    uint32_t ghrBits = ghr & ((1 << ghistoryBits) - 1);
    uint32_t location = ghrBits ^ pcBits;
    uint8_t previous = get_predictor(gPredictors, location);
    if (outcome && previous < 3)
    {
      set_predictor(gPredictors, location, previous + 1);
    }
    else if (previous > 0)
    {
      set_predictor(gPredictors, location, previous - 1);
    }

    // used to be before counter updates
    ghr <<= 1;
    ghr += outcome;
  }
  // Start Tournament training
  else if (bpType == TOURNAMENT)
  {

    uint32_t pcBits = pc & ((1 << pcIndexBits) - 1);
    uint32_t ghrBits = ghr & ((1 << ghistoryBits) - 1);
    uint32_t lhBits = ghr & ((1 << lhistoryBits) - 1);

    // pc -> pattern -> 2-bit counter
    /*
    ====================
    pc 0 -> TNTTTN(?) -> WN
    pc 1 -> NTNTNT(?) -> ST
    pc 2 -> TTTNNT(?) -> WT
    */

    // update local prediction
    uint16_t *localHist = &(lHistory[pcBits]);
    uint16_t localPred = *localHist & ((1 << lhistoryBits) - 1);
    uint8_t previous = get_predictor(lPredictors, localPred);
    if (outcome && previous < 3)
    {
      set_predictor(lPredictors, localPred, previous + 1);
    }
    else if (previous > 0)
    {
      set_predictor(lPredictors, localPred, previous - 1);
    }

    // update local history (PHT)
    *localHist <<= 1;
    *localHist += outcome;

    // update global prediction
    previous = get_predictor(gPredictors, ghrBits);

    if (outcome && previous < 3)
    {
      set_predictor(gPredictors, ghrBits, previous + 1);
    }
    else if (previous > 0)
    {
      set_predictor(gPredictors, ghrBits, previous - 1);
    }

    // update choice prediction if global and local differ

    // reduce to T and NT
    uint8_t local_taken = (last_local > WN);
    uint8_t global_taken = (last_global > WN);

    if (local_taken != global_taken)
    {
      previous = get_predictor(cPredictors, ghrBits);
      // if local was right, try to decrement
      if (outcome == local_taken && previous > 0)
      {
        set_predictor(cPredictors, ghrBits, previous - 1);
        // otherwise global was right, try to increment
      }
      else if (previous < 3)
      {
        set_predictor(cPredictors, ghrBits, previous + 1);
      }
    }
    // update GHR
    ghr <<= 1;
    ghr += outcome;
  }
  // Start perceptron training
  else if (bpType == CUSTOM) {
    // select entry in perceptron table
    uint32_t pcBits = pc & ((1 << pcIndexBits) - 1);
    uint32_t ghrBits = ghr & ((1 << ghistoryBits) - 1);
    pcBits ^= ghrBits;

    // grabs old 
    int y_sign = (y > 0) ? 1 : 0;

    if (outcome != y_sign || (abs(y) <= THETA)) {
      if (ptrons[pcBits][0] < abs(THRESH)) {
        if(outcome == 1) {
          ptrons[pcBits][0]++;
        } else {
          ptrons[pcBits][0]--;
        }
      }

      for (int i = 1; i < P_WEIGHTS; i++) {

        if (ptrons[pcBits][i] < abs(THRESH)) {
          if (((ghr >> i)&1) == 1) {
            if(outcome == 1) {
              ptrons[pcBits][i]++;
            } else {
              ptrons[pcBits][i]--;
            }
          }
        }
      }

    }

    ghr = ghr << 1;
    ghr = ghr + outcome;
  }
  else
  {
    fprintf(stderr, "cant blame us\n");
    exit(1);
  }
}
