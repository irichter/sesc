#if !(defined _ABI_DEFS_H_)
#define _ABI_DEFS_H_

template<ExecMode mode>
class ABIDefs{
};

#include "ABIDefsMips32.h"
#include "ABIDefsMipsel32.h"
#include "ABIDefsMips64.h"
#include "ABIDefsMipsel64.h"

#endif // !(defined _ABI_DEFS_H_)
