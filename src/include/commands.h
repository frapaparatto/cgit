#ifndef CGIT_COMMANDS_H
#define CGIT_COMMANDS_H

int handle_init(int argc, char *argv[]);
int handle_cat_file(int argc, char *argv[]);
int handle_hash_object(int argc, char *argv[]);
int handle_ls_tree(int argc, char *argv[]);
int handle_write_tree(int argc, char *argv[]);

#endif
