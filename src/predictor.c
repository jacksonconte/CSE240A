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

//
//TODO: Add your own Branch Predictor data structures here
//
uint8_t* gPredictors;
uint8_t* ghr;



//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//
void
init_predictor()
{
  // printf("bit sad init\n");
  ghistoryBits = 8;
  pcIndexBits = ghistoryBits;
  int numPredictors = 1 << ghistoryBits; // 2^8 = 256
  // printf("%d\n", numPredictors);
  gPredictors = malloc(numPredictors / 4); // 256 / 4 = 64 bytes
  ghr = malloc(ghistoryBits / 8); // 8/8 = 1
  
  // initialize all to strongly not taken
  for(int i = 0; i < numPredictors / 4; i++) gPredictors[i] = SN;

}

uint8_t
get_gpredictor(int i)
{
  uint8_t fourPredictors = gPredictors[i / 4];
  uint8_t ind = i % 4;
  // chop off last two bits
  return ( fourPredictors >> (2 * ind) ) & 0b11;
}
// 0 <= state <= 3
void
set_gpredictor(int i, uint8_t state)
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
  //
  //TODO: Implement prediction scheme
  //

  uint32_t pcBits = pc & ( (1 << pcIndexBits) - 1);
  uint32_t ghrBits = *ghr;
  uint32_t ind = pcBits ^ ghrBits;
  uint8_t state = get_gpredictor(ind);

  // Make a prediction based on the bpType
  switch (bpType) {
    case STATIC:
      return TAKEN;
    case GSHARE:
      // get last n bits, n = pcIndexBits
      if (state > 1) return TAKEN;
      else return NOTTAKEN;
    case TOURNAMENT:
      return TAKEN;
    case CUSTOM:
      return TAKEN;
    default:
      break;
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
  //
  //TODO: Implement Predictor training for others
  //

  *ghr = (*ghr << 1) + outcome;

  uint8_t location = ((*ghr)^pc);
  uint8_t previous = get_gpredictor(location);
  if (outcome) {// if outcome is true just add 1 unless its already value 11
    if(previous < 3)
      set_gpredictor(location, previous+1);
  } 
  else {
    if(previous > 0)
      set_gpredictor(location, previous-1);
  }
  
}
