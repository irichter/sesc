#include "AdvancedStats.h"

namespace Stats{

  void Group::reportMiddle(size_t level) const{
    for(GroupList::const_iterator groupIt=groupOrder.begin();
	groupIt!=groupOrder.end();groupIt++){
      (*groupIt)->report(level+myName?1:0);
    }
  }
  
  void Group::addSample(const double value) const{
    for(GroupList::const_iterator groupIt=groupOrder.begin();
	groupIt!=groupOrder.end();groupIt++){
      (*groupIt)->addSample(value);
    }
  }
  
  void Group::addSamples(const double value, const LargeCount count) const{
    for(GroupList::const_iterator groupIt=groupOrder.begin();
	groupIt!=groupOrder.end();groupIt++){
      (*groupIt)->addSamples(value,count);
    }
  }
  
  Group::~Group(void){
    while(!groupOrder.empty()){
      Group *nextMember=groupOrder.front();
      delete nextMember;
      I(!groupMembers.count(nextMember));
    }
    if(parentGroup){
      parentGroup->eraseMember(this);
    }
  }
  
  void Distribution::reportMiddle(size_t level) const{
    LargeCount totalCount=0;
    double     totalSum=0;
    // Get the total number of samples
    for(SampleCounts::const_iterator countsIt=sampleCounts.begin();
	countsIt!=sampleCounts.end();countsIt++){
      totalCount+=countsIt->second;
      totalSum+=countsIt->first*countsIt->second;
    }
    indent(level);
    printf("Count:\t%llu\n",totalCount);
    if(totalCount){
      indent(level);
      printf("Averg:\t%lg\n",totalSum/totalCount);
      // Print the percentile points of the distribution
      indent(level);
      printf("Pcntl:");
      size_t percPoint;
      for(percPoint=0;percPoint<=numPoints;percPoint++){
	double percValue;
	if(percPoint<numPoints){
	  percValue=(double)percPoint/(double)numPoints;
	}else{
	  percValue=1;
	}
	printf("\t%lg",percValue);
      }
      printf("\n");
      indent(level);
      printf("Value:");
      percPoint=0;
      LargeCount currentCount=0;
      SampleCounts::const_iterator sampleIt=sampleCounts.begin();
      while(percPoint<=numPoints){
	double percValue;
	LargeCount countLimit;
	if(percPoint<numPoints){
	  percValue=(double)percPoint/(double)numPoints;
	  countLimit=(LargeCount)(percValue*(double)totalCount);
	}else{
	  percValue=1;
	  countLimit=totalCount-1;
	}
	while(currentCount+sampleIt->second<=countLimit){
	  currentCount+=sampleIt->second;
	  sampleIt++;
	}
	printf("\t%lg",sampleIt->first);
	percPoint++;
      }
      printf("\n");
    }
  }

  void Distribution::addSample(const double value){
    typedef std::pair<SampleCounts::iterator,bool> InsertResult;
    InsertResult insertResult=sampleCounts.insert(SampleCounts::value_type(value,1));
    if(!insertResult.second){
      insertResult.first->second++;
    }
    I(groupMembers.empty());
  }

  void Distribution::addSamples(const double value, const LargeCount count){
    typedef std::pair<SampleCounts::iterator,bool> InsertResult;
    InsertResult insertResult=sampleCounts.insert(SampleCounts::value_type(value,count));
    if(!insertResult.second){
      insertResult.first->second+=count;
    }
    I(groupMembers.empty());
  }
}
