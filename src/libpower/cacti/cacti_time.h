#ifdef __cplusplus
extern "C" {
#endif
#ifndef CACTI_TIME_H
#define CACTI_TIME_H
#include "def.h"
#include "areadef.h"
extern void ca_calculate_time(result_type *result, arearesult_type *arearesult, area_type *arearesult_subbanked, parameter_type *parameters, double *NSubbanks) ;
extern double logtwo(double x) ;
#endif
#ifdef __cplusplus
}
#endif
