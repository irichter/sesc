#ifndef Already_included_AdvancedStats_hpp
#define Already_included_AdvancedStats_hpp

#include <map>
#include <list>
#include <string>
#include <iostream>
#include <nanassert.h>

using namespace std;

namespace Stats{
  
  typedef long long unsigned int LargeCount;

  class Group{
  protected:
    static void indent(size_t num){
      for(size_t i=0;i<num;i++)
	printf("  ");
    }
    Group *parentGroup;
    char *myName;
    typedef list<Group *> GroupList;
    GroupList groupOrder;
    typedef GroupList::iterator GroupListIt;
    typedef map<Group *,GroupListIt> GroupMap;
    GroupMap groupMembers;
    virtual void reportPrefix(size_t level) const{
      if(myName){
	indent(level);
	printf("Statistics for %s begin\n",myName);
      }
    }
    virtual void reportMiddle(size_t level) const;
    virtual void reportSuffix(size_t level) const{
      if(myName){
	indent(level);
	printf("Statistics for %s end\n",myName);
      }
    }
  public:
    Group(Group *parentGroup=0, char *name=0)
      : parentGroup(parentGroup),
	myName(name?strdup(name):0){
      if(parentGroup)
	parentGroup->insertMember(this);
    }
    virtual void insertMember(Group *newMember){
      I(!groupMembers.count(newMember));
      GroupListIt listIt=groupOrder.insert(groupOrder.end(),newMember);
      groupMembers.insert(GroupMap::value_type(newMember,listIt));
    }
    virtual void eraseMember(Group *newMember){
      GroupMap::iterator mapIt=groupMembers.find(newMember);
      I(mapIt!=groupMembers.end());
      groupOrder.erase(mapIt->second);
      groupMembers.erase(mapIt);
    }
    virtual void addSample(const double value) const;
    virtual void addSamples(const double value, const LargeCount count) const;
    virtual ~Group(void);
    virtual void report(size_t level=0) const{
      reportPrefix(level);
      reportMiddle(level);
      reportSuffix(level);
    }
  };
  
  class Distribution : public Group{
  protected:
    size_t numPoints;
    typedef std::map<double,LargeCount> SampleCounts;
    SampleCounts sampleCounts;
    LargeCount totalCount;
    virtual void reportMiddle(size_t level) const;
  public:
    Distribution(Group *parentGroup=0, char *name=0,size_t numPoints=100)
      : Group(parentGroup,name), numPoints(numPoints){
    }
    virtual void addSample(const double value);
    virtual void addSamples(const double value, const LargeCount count);
  };
  
}
#endif
