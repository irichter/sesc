#if !(defined ELF_OBJECT_H)
#define ELF_OBJECT_H

class ThreadContext;

void loadElfObject(const char *fname, ThreadContext *threadContext);

#endif
