#include <stdio.h>
#include <string.h>

#include "include/commands.h"

typedef struct {
  const char *name;
  int (*handler)(int argc, char *argv[]);
  const char *usage;
} command_t;

static const command_t commands[] = {
    {"init", handle_init, "cgit init"},
    {"cat-file", handle_cat_file,
     "cgit cat-file <type | (-p | -t | -e | -s)> <object>"},
    {"hash-object", handle_hash_object, "cgit hash-object [-w] <file>"},
    {"ls-tree", handle_ls_tree, "cgit ls-tree [--name-only] <object>"},
    {"write-tree", handle_write_tree, "cgit write-tree"},
    {"commit-tree", handle_commit_tree,
     "cgit commit-tree <tree-hash> [-p <parent-hash>] -m <commit-message>"},
    {NULL, NULL, NULL}};

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: cgit <command>\n");
    return 1;
  }

  /* Handler receives also the command name in order to use it for better error
   * messages */
  for (int i = 0; commands[i].name; i++) {
    if (strcmp(argv[1], commands[i].name) == 0)
      return commands[i].handler(argc - 1, argv + 1);
  }

  fprintf(stderr, "Unknown command: %s\n", argv[1]);
  return 1;
}
