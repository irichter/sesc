
#ifndef SIGNATURE_H
#define SIGNATURE_H


#define OUTORDER 0
#define INORDER 1

#define STABLE 0
#define TRAINING 1

#define WINDOW_SIZE 10000

//==================================================================
//
//==================================================================
class SignatureVector {
 private:
  unsigned int signator[16];
 public:
  SignatureVector();
  ~SignatureVector();
  
  unsigned int* getSignature();
  
  int  hashPC(unsigned int pc);

  void setPCBit(unsigned int pc); // Calls hashPC
  void setBit(int index);

  int getNumberOfBits();
  int getNumberOfBits(unsigned int input);
  int matchBits(unsigned int compareSig[]);

  float getSignatureDifference(unsigned int compareSig[]);
  int copySignature(unsigned int copySig[]);
  int clearBits();
};


//==================================================================
//
//==================================================================
class SignatureEntry{
public:
  SignatureVector sigVec;
  int mode;
  int taken;
  int accessTimes; // For LRU policy (to be implemented )
  
  SignatureEntry(){               
    mode = -1;
    taken = 0;
    accessTimes = 0;
  }
  
  ~SignatureEntry(){
  }
};

class SignatureTable {
 private:
  SignatureEntry sigEntries[128];
  int nextUpdatePos;

 public:
  /* Return -1, index in array */
  SignatureTable();
  ~SignatureTable();
  int isSignatureInTable(SignatureVector signature);
  int addSignatureVector(SignatureVector signature, int mode);
  int getSignatureMode(int index);
};

//==========================================================================
//
//==========================================================================
class PipeLineSelector {
 private:

  SignatureVector signatureCurr;
  SignatureVector signaturePrev;
  SignatureTable table;
  
  double totEnergy;
  int clockCount;
  double inOrderEDD, outOrderEDD; // Energy * delay^2
  
  int windowInstCount; // counter from 0 - 10,000
  int trainWindowCount;  
  int currPipelineMode, currState; 
  
  void updateCurrSignature(unsigned int pc);
  double calculateEDD(int currClockCount, double currTotEnergy);
        
 public:
  PipeLineSelector();
  ~PipeLineSelector();
  int getPipeLineMode(unsigned int pc, int currClockCount, double currTotEnergy); // called on every clock
};

#endif   // SIGNATURE_H

