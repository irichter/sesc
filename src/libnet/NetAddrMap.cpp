#include "NetAddrMap.h"

NetAddrMap::MapType NetAddrMap::type;
ulong NetAddrMap::log2TileSize = 10;
ulong NetAddrMap::numNodes = 1;
ulong NetAddrMap::numNodesMask = 0x0;
