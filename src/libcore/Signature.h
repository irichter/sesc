
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
class SignatureVector{
 private:
  unsigned long signator[16];
 public:
  SignatureVector();
  ~SignatureVector();
  
  unsigned long* getSignature();
  
  int hashPC(unsigned long pc);
  int setPCBit(unsigned long pc); // Calls hashPC
  int setBit(int index);
  int getNumberOfBits();
  int getNumberOfBits(unsigned long input);
  int matchBits(unsigned long compareSig[]);
  float getSignatureDifference(unsigned long compareSig[]);
  int copySignature(unsigned long copySig[]);
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



class SignatureTable{
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
class PipeLineSelector{
 private:

        SignatureVector signatureCurr;
        SignatureVector signaturePrev;
        SignatureTable table;

        double totEnergy;
        long clockCount;
        double inOrderEDD, outOrderEDD; // Energy * delay^2
        
        long windowInstCount; // counter from 0 - 10,000
        int trainWindowCount;  
        int currPipelineMode, currState; 

        void updateCurrSignature(unsigned long pc);
        double calculateEDD(long currClockCount, double currTotEnergy); 						 

        
 public:
   PipeLineSelector();
   ~PipeLineSelector();
   int getPipeLineMode(unsigned long pc, long currClockCount, double currTotEnergy); // called on every clock

};

#endif   // SIGNATURE_H

