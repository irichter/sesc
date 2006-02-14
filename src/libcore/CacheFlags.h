#ifndef CACHEFLAGS_H
#define CACHEFLAGS_H
//#include "Epoch.h"
namespace tls{
	class Epoch;
	
	typedef long int ClockValue;
	//Template disabled because of vector object container problems
	//template <unsigned int blksize=32, unsigned int wordsize=4>
	class CacheFlags
	{
	  static const int blksize=32;
	  static const int wordsize=4;
	  protected:
	   unsigned short int wmFlags[blksize/wordsize];
	   unsigned short int erFlags[blksize/wordsize];
	   unsigned char oeFlags;
	   Epoch *epoch; 
	public:
		CacheFlags() 
		{
			int allocsize=(blksize/wordsize);
			epoch=0;
			oeFlags=0;
			for (int i=0;i<allocsize;i++)
			{
				wmFlags[i]=0;
				erFlags[i]=0;
			}
		}
		//Copy Constructor
		CacheFlags(const CacheFlags &cp)
		{
			int allocsize=(blksize/wordsize);
			epoch=cp.getEpoch();
			oeFlags=cp.getOE();
			for (int i=0;i<allocsize;i++)
			{
				wmFlags[i]=cp.getWordWM(i);
				erFlags[i]=cp.getWordER(i);
			}
		}
		CacheFlags& operator=(const CacheFlags& cf) 
		{
			int allocsize=(blksize/wordsize);
			epoch=cf.getEpoch();
			oeFlags=cf.getOE();
			for (int i=0;i<allocsize;i++)
			{
				wmFlags[i]=cf.getWordWM(i);
				erFlags[i]=cf.getWordER(i);
			}
			return *this;
		}
		bool operator==(const CacheFlags& cf) const
		{
			int allocsize=(blksize/wordsize);
			if (epoch!=cf.getEpoch() || oeFlags!=cf.getOE())
				return false;
			for (int i=0;i<allocsize;i++)
			{
				if (wmFlags[i]!=cf.getWordWM(i) ||erFlags[i]!=cf.getWordER(i))
					return false;
			}
			return true;
		}
		bool operator!=(const CacheFlags& cf) const
		{
			int allocsize=(blksize/wordsize);
			if (epoch!=cf.getEpoch() || oeFlags!=cf.getOE())
				return true;
			for (int i=0;i<allocsize;i++)
			{
				if (wmFlags[i]!=cf.getWordWM(i) ||erFlags[i]!=cf.getWordER(i))
					return true;
			}
			return false;
		}
		~CacheFlags()
		{
			//delete erFlags;
			///elete wmFlags;
		}
		void setEpoch(Epoch* myepoch){
			I(myepoch);
			epoch=myepoch;
			}
		void setWordER(long int pos)
		{
			//pos is assumed to be byte offset from 1st byte of line
			long int pos1=pos/(long)wordsize;
			if (wmFlags[pos1]!=1)
			{
				erFlags[pos1]=1;
			}
		}
		
		void setWordWM(long int pos)
		{
			long int pos1=pos/(long)wordsize;
			wmFlags[pos]=1;
		}
		
		void setOE(void)
		{
			oeFlags=1;
		}
		void clearOE(void)
		{
			if (oeFlags==1) 
			//printf("OE Flag reset\n");
			oeFlags=0;
		}
		
		unsigned int getWordER(int pos) const
		{
			long int pos1=pos/(long)wordsize;
			return  erFlags[pos1];
		}
		
		unsigned int getWordWM(int pos) const
		{
			long int pos1=pos/(long)wordsize;
			return wmFlags[pos1];
		}
		
		unsigned int getOE(void) const
		{
			return oeFlags;
		}
		//ClockValue getEpochNum()const {return epoch->getClock();}
		Epoch * getEpoch() const{return epoch;}
		void clearFlags()
		{
			unsigned i;
			for (i=0;i<(blksize/wordsize);i++)
			{
				erFlags[i]=0;
				wmFlags[i]=0;
			}
			oeFlags=0;
			epoch=0;
			
		}
	};
}
#endif
