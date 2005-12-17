#ifndef __symtab_h
#define __symtab_h

#ifndef FUNC_HASH_SIZE
#define FUNC_HASH_SIZE 128
#endif

typedef struct func_name_t {
  unsigned int addr;
  struct func_name_t *next;
  const char *fname;
} func_name_t, *func_name_ptr;

#define FUNC_ENTRY_SIZE (sizeof(struct func_name_t))

typedef struct namelist {
  const char *n_name;
  int n_type;
  unsigned int n_value;
} namelist_t, *namelist_ptr;


int namelist(char *objname, namelist_ptr pnlist);
void read_hdrs(char *objfile);
void close_object();

struct file_info {
  int addr;
  char *fname;
  int linelow;
  unsigned char *lptr;
};

#endif /* __symtab_h */
