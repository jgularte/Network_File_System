// CPSC 3500: File System
// Implements the file system commands that are available to the shell.

#ifndef FILESYS_H
#define FILESYS_H

#include "BasicFileSys.h"
#include "Blocks.h"
#include <string>
#include <string.h>

class FileSys {
  
  public:
    // mounts the file system
    void mount(int sock);

    // unmounts the file system
    void unmount();

    // make a directory
    void mkdir(const char *name);

    // switch to a directory
    void cd(const char *name);
    
    // switch to home directory
    void home();
    
    // remove a directory
    void rmdir(const char *name);

    // list the contents of current directory
    void ls();

    // create an empty data file
    void create(const char *name);

    // append data to a data file
    void append(const char *name, const char *data);

    // display the contents of a data file
    void cat(const char *name);

    // display the first N bytes of the file
    void head(const char *name, unsigned int n);

    // delete a data file
    void rm(const char *name);

    // display stats about file or directory
    void stat(const char *name);
	
	

  private:
  
	//objects
    BasicFileSys bfs;	// basic file system
	
	// primitives
    short curr_dir;	// current directory
    int fs_sock;  // file server socket

	//functions
	short get_dir_entry(dirblock_t *directory);
	int get_first_entry(inode_t *inode);
	int find_directory(dirblock_t *current, dirblock_t *inner, char* name);
	int find_file(dirblock_t *current, inode_t *inner, char* name);
	bool send_message(std::string s);
	void push_up(dirblock_t* ptr, int x);
	int find_append_file(dirblock_t *current, inode_t *inner, char* name);
	
};

#endif 
