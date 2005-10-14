#ifndef THERM_TRACE_H_
#define THERM_TRACE_H_

#include <vector>

#include "nanassert.h"

class ThermTrace {
 private:
  const char *input_file_;
  int input_fd_;

  typedef std::vector<const char *> TokenVectorType;

  // Floorplan
  class FLPUnit {
  public:
    FLPUnit() {
      area  = 0;
      units = 0;
      name  = 0;
    }
    void dump() const {
      MSG("Block %20s area %4.2g #units %d",name, area*1e6, units);
    }

    const char *name;
    float area;
    int units;
    float energy;
  };
  std::vector<FLPUnit> flp;

  class Mapping {
  public:
    Mapping() {
      area = 0;
      name = 0;
    }
    void dump() const {
      fprintf(stderr,"Mapping %45s area %4.2g map:", name, area*1e6);
      for(size_t j=0; j<map.size(); j++) {
	fprintf(stderr," %3d (%3.0f%%)", map[j], 100*ratio[j]);
      }
      fprintf(stderr,"\n");
    }
    
    const char *name;
    float area;
    std::vector<int>   map;
    std::vector<float> ratio;
  };
  std::vector<Mapping> mapping;

  static void tokenize(const char *str, TokenVectorType &tokens);
  static bool grep(const char *line, const char *pattern);

  void read_sesc_variable(const char *input_file);
  void read_floorplan_mapping();

 protected:
 public:
  ThermTrace(const char *input_file);

  bool read_energy();
  
  float get_energy(size_t pos) const {
    I(pos < flp.size());
    return flp[pos].energy;
  }
  size_t get_energy_size() const { return flp.size(); }

  void dump() const;
};

#endif
