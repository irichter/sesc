#include "Signature.h"

#include <stdio.h>

SignatureVector::SignatureVector(){
  // Clear all bits
  for(int i = 0; i < 16; i++){
    signator[i] = 0;
  }// End Array
}

SignatureVector::~SignatureVector(){}


int SignatureVector::setPCBit(unsigned long pc){
  int index = hashPC(pc);

  if(setBit(index) != 1)
    printf("Error SignatureVector::setPCBit could not set PC bit\n");

  return 1;
}


int SignatureVector::hashPC(unsigned long pc){
  unsigned int temp9   ;
  unsigned int temp3High ;
  unsigned int temp3Mid ;
  unsigned int temp3Low ;
  unsigned int resultTri ;
  unsigned int result   ;

  result = 0;

  // Get Higher order bits (highest 9 bits)
  temp9 = (pc & 0xFF800000) >> 23;
  // Split 9 bit into 3 catigories
  temp3High = (temp9 & 0x1C0) >> 6;
  temp3Mid = (temp9 & 0x38) >> 3;
  temp3Low = (temp9 & 0x7);
  // Get 3 bit pattern
  resultTri = (temp3High ^ temp3Mid) ^ temp3Low;
   result = result | (resultTri << 6);

  // Get Mid order bits (highest 9 bits)
  temp9 = (pc & 0x007FC000) >> 14;
  // Split 9 bit into 3 catigories
  temp3High = (temp9 & 0x1C0) >> 6;
  temp3Mid = (temp9 & 0x38) >> 3;
  temp3Low = (temp9 & 0x7);
  // Get 3 bit pattern
  resultTri = (temp3High ^ temp3Mid) ^ temp3Low;
  result = result | (resultTri << 3);

  // Get low order bits (highest 9 bits)
  temp9 = (pc & 0x00006FFF)  >> 5;
  // Split 9 bit into 3 catigories
  temp3High = (temp9 & 0x1C0) >> 6;
  temp3Mid = (temp9 & 0x38) >> 3;
  temp3Low = (temp9 & 0x7);
  // Get 3 bit pattern
  resultTri = (temp3High ^ temp3Mid) ^ temp3Low;
  result = result | resultTri;

  return result;
}



int SignatureVector::setBit(int index){

  int  arrayIndex = 0;
  int subIndex = 0;
  int range_count = 32;

  // Error check range
  if(index > 511)
    return -1;

  // Get array index (0-15)
  for(arrayIndex = 0; arrayIndex < 16; arrayIndex++){
    if (index < range_count)
      break;
    range_count += 32;
  }                                                       
  // Get index in subArray
  subIndex = index % (range_count - 32);

  // Set Bit to onern 
  signator[arrayIndex] = signator[arrayIndex] | (1 << subIndex);
  
  return 1;
}

int SignatureVector::getNumberOfBits(){

  int bitCount = 0;
  long tempLong = 0;
  for(int i = 0; i < 16; i++){
    tempLong =signator[i];
    for(int j = 0; j < 32; j++){
      if(tempLong & 0x80000000){
        bitCount++;
      }
      tempLong = tempLong << 1;
    }// End long
  }// End Array
  
  return 1;
}

int SignatureVector::getNumberOfBits(unsigned long input){

  int bitCount = 0;
  unsigned long tempLong = 0;
 
    tempLong =input;
    for(int j = 0; j < 32; j++){
      if(tempLong & 0x1){
        bitCount++;
      }
      tempLong = tempLong >> 1;
    }// End long

  return bitCount;
}

float SignatureVector::getSignatureDifference(unsigned long compareSig[]){

  float percentDif = 0.0;
  int bitCountCommon = 0;
  int bitCountDiff = 0;

  unsigned long bitsCommon[16];
  unsigned long bitsDiff[16];

//    long tempLong = 0;
 // long tempComp = 0;

  for(int i = 0; i < 16; i++){
    bitsDiff[i]   = signator[i] ^ compareSig[i];
    bitsCommon[i] = signator[i] & compareSig[i];
    
    bitCountCommon += getNumberOfBits(bitsCommon[i]);
    bitCountDiff   += getNumberOfBits(bitsDiff[i]);
    
  }// End For

 
  if(bitCountCommon == 0)
  	return 1.0;
  	
  percentDif = (float)bitCountDiff / (float)bitCountCommon;

  return percentDif;
}

unsigned long* SignatureVector::getSignature(){
	return signator;
}

int SignatureVector::matchBits(unsigned long compareSig[16]){

  unsigned long tempLong;
  unsigned long tempComp;
  for(int i = 0; i < 16; i++){
    tempLong = signator[i];
    tempComp = compareSig[i];
      if(tempLong ^ tempComp){
        return -1;
      }
  }// End Array

  return 1;
}

int SignatureVector:: copySignature(unsigned long copySig[]){
  for(int i = 0; i < 16; i++){
    signator[i] = copySig[i];
  }// End Array
  
  return 1;
}

int SignatureVector:: clearBits(){
  for(int i = 0; i < 16; i++){
    signator[i] = 0;
  }// End Array
  
  return 1;
}


//======================================
SignatureTable::SignatureTable(){
        nextUpdatePos = 0;
}

SignatureTable::~SignatureTable(){
        for(int i = 0; i < 128; i++){
             //   delete(&sigEntries[i]);
        }
}

int SignatureTable::isSignatureInTable(SignatureVector vec){
        SignatureEntry *entry;
        for(int i = 0; i < 128; i++){
                entry = &sigEntries[i];
                if(entry->taken != 0){
                        if(entry->sigVec.matchBits(vec.getSignature())){
                                return i;
                        }
                }
        }
        return -1;
}


int SignatureTable::getSignatureMode(int index){
        SignatureEntry *entry = &sigEntries[index];
        if(entry->taken == 0)
                return -1;
        else
                return entry->mode;
}

int SignatureTable::addSignatureVector(SignatureVector vec, int mode){

        SignatureEntry *entry = &sigEntries[nextUpdatePos];
        //vec.copySignature(entry->sigVec.getSignature()); // Need to make a copy constructor
        entry->sigVec.copySignature(vec.getSignature());
        entry->taken = 1;
        entry->mode = mode;

        nextUpdatePos = (nextUpdatePos + 1) % 128;
        
        return 1;
}

//===========================================================================
//
//===========================================================================
PipeLineSelector::PipeLineSelector(){
        clockCount  = 0;
        inOrderEDD  = 0;
        outOrderEDD = 0;
        totEnergy = 0;
        windowInstCount = 0;
        currPipelineMode = 0; 
        trainWindowCount = 0;
        currState = 0;
}

PipeLineSelector::~PipeLineSelector(){     
}

//=====================================================
void PipeLineSelector::updateCurrSignature(unsigned long pc){
        signatureCurr.setPCBit(pc);
}


double PipeLineSelector::calculateEDD(long currClockCount, double currTotEnergy){ 
	  double caculatedEDD = 0;
	  //double energy =  GStatsEnergy::getTotalEnergy();
	  
      double delta_energy = currTotEnergy - totEnergy;       
      long  delta_time = currClockCount - clockCount;
	  caculatedEDD = delta_energy * (double)(delta_time * delta_time);
	 
	  return caculatedEDD;
	
}

//====================================================================================
int PipeLineSelector::getPipeLineMode(unsigned long pc, long currClockCount, double currTotEnergy){ // called on every pc dispatch
	float signatureDiff = 0.0;
    int tableIndex;

    if(pc == 0){
      return currPipelineMode;
    }
    //printf("getpipeLineMode: %x\n", pc); 
    // Update currenect signature with current pc and windowInstCount
    updateCurrSignature(pc);
    ++windowInstCount;
    //if((windowInstCount % 100) == 0)
     	printf("getpipeLineMode: %x Count:%d\n", pc, windowInstCount);  
  
    //printf("Updated Window\n"); 
     // Check if we are at an end of a window interval
    if(windowInstCount == 10){
        windowInstCount = 0;  
    	// We can calclulate the edd for the last window
    	if(currPipelineMode == INORDER){
    		inOrderEDD = calculateEDD(currClockCount, currTotEnergy); 
    	}else{
    		outOrderEDD = calculateEDD(currClockCount, currTotEnergy); 
    	}	
    		
    	// Get Signature difference
        signatureDiff = signaturePrev.getSignatureDifference(signatureCurr.getSignature());
                        
        if(signatureDiff > 0.5){
          	//==========================
            // Phase Change Detected
            //=========================
            printf("Phase change detected\n");          
            // Check if signature is in table
            if((tableIndex = table.isSignatureInTable(signatureCurr)) == 1){
                // Signature found
                printf("Found Siganature\n");
                currPipelineMode = table.getSignatureMode(tableIndex);  
                currState = STABLE;             
                
            }else{
                if(currState == STABLE){
                    // Signature NOT FOUND! (NEED TO TRAIN)
                    // reset statiscsi
                    printf("Start training\n");
                    currState = TRAINING;
                    currPipelineMode = INORDER;
                    trainWindowCount = 0;

                }else{
                	// Already Training  (need to reset training) 
                	// reset statiscs
                   printf("Start trianing\n"); 
		    currState = TRAINING;
                    currPipelineMode = INORDER;
                    trainWindowCount = 0;                                             
                }
            }// END Signature found 
          
                           
        }else{
           //==========================
           // NO Phase Change
           //=========================
           if(currState == TRAINING){
             switch(trainWindowCount){
               case 0:
                 // capture statiics
                 printf("Training Step 0\n");
                 clockCount = currClockCount;
                 totEnergy = currTotEnergy;
                 currPipelineMode = INORDER;
                 break;
               case 1:
                 printf("Training Step 1\n");
               	 // compute inorder edd, swithc to outorder  	
               	 inOrderEDD = calculateEDD(currClockCount, currTotEnergy);               	  
               	 currPipelineMode = OUTORDER;
                 break;
               case 2:
                 printf("Traing Step 2\n");
               	 // capture statistics
               	 clockCount = currClockCount;
                 totEnergy = currTotEnergy;
                 currPipelineMode = OUTORDER;               
                 break;
               case 3:
                 printf("Training Step 3\n");
               	 // compute out of order edd, find best, add to table
                 outOrderEDD = calculateEDD(currClockCount, currTotEnergy);   
                 if(inOrderEDD < outOrderEDD){                 	
                 	currPipelineMode = INORDER;
                 }else{
                 	currPipelineMode = OUTORDER;
             	 }
             	 // Add signuter to table
             	 table.addSignatureVector(signatureCurr, currPipelineMode);	
                 break;
            
              
            }// End Switch
            ++trainWindowCount;                                                        
          }   
      }/* EnD No Phase Change */
      
      // Copy curr signister to prev, reset currentSignature fo rnew window
      signaturePrev.copySignature(signatureCurr.getSignature()); 
      signatureCurr.clearBits();    
      windowInstCount = 0;  
   }   
   return currPipelineMode;
}


