/*
 * Common definitions used by other header files
 */

#ifndef __common_h
#define __common_h

typedef int (*PFI)(void *,...);
typedef void (*PFV)(void *,...);

void fatal(char *s, ...);
void error(char *fmt, ...);
void warning(char *fmt, ...);

#endif /* !__common_h */
