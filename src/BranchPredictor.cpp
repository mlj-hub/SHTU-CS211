/*
 * Created by He, Hao on 2019-3-25
 */

#include "BranchPredictor.h"
#include "Debug.h"

BranchPredictor::BranchPredictor(int32_t numHistory, int32_t numPerce,int32_t threshold):
perceptron(numHistory+1,numPerce,threshold) {
  for (int i = 0; i < PRED_BUF_SIZE; ++i) {
    this->predbuf[i] = WEAK_TAKEN;
  }
  // build the history table 
  this->historyTable.resize(numHistory);
  this->numHistory = numHistory;
}

BranchPredictor::~BranchPredictor() {}

bool BranchPredictor::predict(uint32_t pc, uint32_t insttype, int64_t op1,
                              int64_t op2, int64_t offset) {
  switch (this->strategy) {
  case NT:
    return false;
  case AT:
    return true;
  case BTFNT: {
    if (offset >= 0) {
      return false;
    } else {
      return true;
    }
  }
  break;
  case BPB: {
    PredictorState state = this->predbuf[pc % PRED_BUF_SIZE];
    if (state == STRONG_TAKEN || state == WEAK_TAKEN) {
      return true;
    } else if (state == STRONG_NOT_TAKEN || state == WEAK_NOT_TAKEN) {
      return false;
    } else {
      dbgprintf("Strange Prediction Buffer!\n");
    }   
  }
  break;
  case PERCEPTRON:{
    // update lastOutcome for tranning 
    this->perceptron.lastOutome = this->perceptron.curOutcome;
    int32_t outcome = 0;
    // get index 
    uint32_t idx = this->perceptron.hash(pc);
    // get weight vector
    std::vector<int32_t> weight = this->perceptron.perceTable[idx];
    // add bias
    outcome += weight[0];
    for(int i =0;i < this->numHistory;i++){
      outcome+=weight[i+1]*this->historyTable[i];
    }
    this->perceptron.curOutcome = outcome;
    if(outcome>=0)
      return true;
    else
      return false;
  }
  default:
    dbgprintf("Unknown Branch Perdiction Strategy!\n");
    break;
  }
  return false;
}

void BranchPredictor::update(uint32_t pc, bool branch) {
  switch(this->strategy){
    case BPB:{
      int id = pc % PRED_BUF_SIZE;
      PredictorState state = this->predbuf[id];
      if (branch) {
        if (state == STRONG_NOT_TAKEN) {
          this->predbuf[id] = WEAK_NOT_TAKEN;
        } else if (state == WEAK_NOT_TAKEN) {
          this->predbuf[id] = WEAK_TAKEN;
        } else if (state == WEAK_TAKEN) {
          this->predbuf[id] = STRONG_TAKEN;
        } // do nothing if STRONG_TAKEN
      } else { // not branch
        if (state == STRONG_TAKEN) {
          this->predbuf[id] = WEAK_TAKEN;
        } else if (state == WEAK_TAKEN) {
          this->predbuf[id] = WEAK_NOT_TAKEN;
        } else if (state == WEAK_NOT_TAKEN) {
          this->predbuf[id] = STRONG_NOT_TAKEN;
        } // do noting if STRONG_NOT_TAKEN
      }
      break;
    }
    case PERCEPTRON:{
      // get history according to the outcome
      int8_t t = branch?1:-1;
      // get index to the perceTable
      uint32_t idx = this->perceptron.hash(pc);
      // condition for tranning 
      if(this->perceptron.sign(this->perceptron.lastOutome)!=branch || 
                 abs(this->perceptron.lastOutome)<=this->perceptron.threshold ){
        this->perceptron.perceTable[idx][0]+=t;
        for (int i=0;i<this->numHistory;i++)
          this->perceptron.perceTable[idx][i+1]+=(int32_t)t*this->historyTable[i];
      }
      // update history table
      this->historyTable.pop_front();
      this->historyTable.push_back(t);
      break;
    }
    default:
      break;
  }
  
}

std::string BranchPredictor::strategyName() {
  switch (this->strategy) {
  case NT:
    return "Always Not Taken";
  case AT:
    return "Always Taken";
  case BTFNT:
    return "Back Taken Forward Not Taken";
  case BPB:
    return "Branch Prediction Buffer";
  case PERCEPTRON:
    return "Branch Prediction with Peceptron";
  default:
    dbgprintf("Unknown Branch Perdiction Strategy!\n");
    break;
  }
  return "error"; // should not go here
}

/* Get hash value to index the perceptron table */
uint32_t BranchPredictor::Perceptron::hash(uint32_t pc){
  return pc%this->numPerceptrons;
}

/* Sign function to change outcome to bool */
bool BranchPredictor::Perceptron::sign(int32_t lastOutcome){
  if(lastOutcome>=0)
    return true;
  else
    return false;
}