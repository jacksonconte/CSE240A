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
const char *studentID   = "PID";
const char *email       = "EMAIL";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = { "Static", "Gshare",
                          "Tournament", "Custom" };

int ghistoryBits; // Number of bits used for Global History
int lhistoryBits; // Number of bits used for Local History
int pcIndexBits;  // Number of bits used for PC index
int bpType;       // Branch Prediction Type
int verbose;

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

uint8_t* gPredictors;
uint8_t* lPredictors;
uint8_t* cPredictors; // choice predictions
uint16_t* lHistory;


uint32_t ghr = 0;
uint32_t numPredictors;

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//
void
init_predictor()
{
  printf("========= %s =========\n", bpName[bpType]);
  // initalizes structures for static
  if (bpType == STATIC) {
    return;
  }
  // initializes structures for GShare
  if (bpType == GSHARE) {
    if (ghistoryBits > 31) {
      fprintf(stderr, "ERROR: most bits supported for gshare is 31\n");
      exit(1);
    }
    pcIndexBits = ghistoryBits;
    numPredictors = 1 << ghistoryBits;
    printf("# predictors = %u\n\n", numPredictors);

    gPredictors = malloc(numPredictors / 4); // freed on exit lmao

    // initialize all to strongly not taken
    for(uint32_t i = 0; i < numPredictors / 4; i++) gPredictors[i] = SN;
  } 

  // initalizes structures for Tournament
  else if (bpType == TOURNAMENT) {
    // TODO: check bit #s
    uint32_t numGPredictors = 1 << ghistoryBits;
    gPredictors = malloc(numGPredictors / 4);
    uint32_t numCPredictors = 1 << ghistoryBits;
    cPredictors = malloc(numGPredictors / 4);
    uint32_t numLPredictors = 1 << lhistoryBits;
    lPredictors = malloc(numLPredictors / 4);

    for(uint32_t i = 0; i < numGPredictors / 4; i++) gPredictors[i] = SN;
    for(uint32_t i = 0; i < numGPredictors / 4; i++) cPredictors[i] = SN;
    for(uint32_t i = 0; i < numLPredictors / 4; i++) lPredictors[i] = SN;
  }
  
  // initalizes structures for Custom
  else if (bpType == CUSTOM) {
    
  }

  else {
    fprintf(stderr, "shit's fucked bro\n");
    exit(1);
  }

}
// helper function
uint8_t
get_predictor(uint32_t i)
{
  uint8_t fourPredictors = gPredictors[i / 4];
  uint8_t ind = i % 4;
  // only return last two bits
  return ( fourPredictors >> (2 * ind) ) & 0b11;
}
// helper function
// 0 <= state <= 3
void
set_predictor(uint32_t i, uint8_t state)
{
  uint8_t fourPredictors = gPredictors[i / 4];
  uint8_t ind = i % 4; // 0-3
  uint8_t mask = ~( 0b11 << (2*ind) ); // 2*ind = 4; mask = 11001111
  fourPredictors &= mask;
  fourPredictors |= ( state << (2*ind) ); // 11 00 11 11 | 00 01 00 00
  gPredictors[i / 4] = fourPredictors; // it just works(tm)
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t
make_prediction(uint32_t pc)
{ 
  if(bpType == STATIC) {
    return TAKEN;
  }
  else if(bpType == GSHARE) {
    // mask by index bits
    uint32_t pcBits = pc & ( (1 << pcIndexBits) - 1);
    uint32_t ghrBits = ghr & ( (1 << ghistoryBits) - 1);
    uint32_t ind = pcBits ^ ghrBits;
    uint8_t state = get_predictor(ind);

    if (state > 1) return TAKEN;
    else return NOTTAKEN;
  }
  else if(bpType == TOURNAMENT) {
    // mask by index bits
    uint32_t pcBits = pc & ( (1 << pcIndexBits) - 1);
    uint32_t ghrBits = ghr & ( (1 << ghistoryBits) - 1);
    uint32_t lhBits = ghr & ( (1 << lhistoryBits) - 1);
    
    uint8_t choice = get_predictor(ghrBits);
    // local
    if (choice < 2) {
      return (get_predictor(lhBits) > WN); // return 1 if taken
    } else { // global
      return (get_predictor(ghrBits) > WN);
    }
  }
  else if(bpType == CUSTOM) {
    return TAKEN;
  } 
  else {
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
void
train_predictor(uint32_t pc, uint8_t outcome)
{
  if (bpType == STATIC) {
    return;
  } 
  else if (bpType == GSHARE) {
    ghr<<=1;
    ghr += outcome;

    // Add masking to the pcBits and ghrbits
    uint32_t pcBits = pc & ( (1 << pcIndexBits) - 1);
    uint32_t ghrBits = ghr & ( (1 << ghistoryBits) - 1);
    uint32_t location = ghrBits^pcBits;
    uint8_t previous = get_predictor(location);
    if (outcome && previous < 3) {
        set_predictor(location, previous+1);
    }
    else if(previous > 0) {
        set_predictor(location, previous-1);
    }
  }
  // Start Tournament training
  else if (bpType == TOURNAMENT) {
    ghr <<= 1;
    ghr += outcome;

    

    uint32_t pcBits = pc & ( (1 << pcIndexBits) - 1);
    uint32_t ghrBits = ghr & ( (1 << ghistoryBits) - 1);
    uint32_t lhBits = ghr & ( (1 << lhistoryBits) - 1);

    // update local history table
    // lPredictors[lhBits]

    if (outcome && previous < 3) {
        set_predictor(location, previous+1);
    }
    else if(previous > 0) {
        set_predictor(location, previous-1);
    }
  }
  // Start custom training
  else if (bpType == CUSTOM) {

  }
  else {
    fprintf(stderr, "cant blame us\n");
    exit(1);
  }
}
