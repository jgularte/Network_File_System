// Authors: Jack Gularte and Tony Caballero
// Date: March 11th, 2019
// For: Prof. Zhu; CPSC 3500
// Filename: FileSys.cpp
// Compiles with: make
// Runs with: ./nfsserver $port_number

#include <cstring>
#include <string.h>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string>
#include <string.h>

#include "FileSys.h"
#include "BasicFileSys.h"
#include "Blocks.h"

using namespace std;

// mounts the file system
void FileSys::mount(int sock) {
	bfs.mount();
	curr_dir = 1; //by default current directory is home directory, in disk block #1
	fs_sock = sock; //use this socket to receive file system operations from the client and send back response messages
}

// unmounts the file system
void FileSys::unmount() {
	bfs.unmount();
	close(fs_sock);
}

// make a directory
void FileSys::mkdir(const char *name)
{

	// cast to dirblock_t*
	dirblock_t current; 
	
	// read current directory block
	bfs.read_block(curr_dir, (void *) &current);
	
	// check for name length
	if (strlen(name) > MAX_FNAME_SIZE) {
		send_message("504 File name is too long\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}
	
	for(int i = 0; i < current.num_entries; i++) {
		if (strcmp(name, current.dir_entries[i].name) == 0) {
			send_message("502 File Exists\r\n");
			send_message("Length:0\r\n");
			send_message("\r\n");
			return;
		}
	}
	
	// get a free block
	short new_block = bfs.get_free_block();
	
	if (new_block == 0) {
		send_message("505 Disk is full\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}
	
	// get next directory slot
	short new_entry = get_dir_entry(&current);
	
	// check to make sure dir slot is valid
	// if so, update info of curr_dir
	if (new_entry >= 0) {
		current.dir_entries[new_entry].block_num = new_block;
		current.num_entries++;
		strcpy(current.dir_entries[new_entry].name, name);
	}
	else {
		send_message("506 Directory is full\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}
	
	// initialize the new directory: maybe make own function
	struct dirblock_t new_dir_block;
	new_dir_block.magic = DIR_MAGIC_NUM;
	new_dir_block.num_entries = 0;
	for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
		new_dir_block.dir_entries[i].block_num = 0;
	}
	
	// write updated blocks to disk
	bfs.write_block(curr_dir, (void *) &current);
	bfs.write_block(new_block, (void *) &new_dir_block);
	
	send_message("200 OK\r\n");
	send_message("Length:0\r\n");
	send_message("\r\n");
	
	return;
}

// switch to a directory
void FileSys::cd(const char *name)
{

	// struct to read current directory properities into
	dirblock_t current, inner;
	bfs.read_block(curr_dir, (void *) &current);
	
	// find directory block
	int dir = find_directory(&current, &inner, const_cast<char*> (name));
	
	if(dir == -1){
		send_message("500 File is not a directory\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}
	else if(dir == -2){
		send_message("503 File does not exist\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}
	
	// all tests passed; moving into direcotry
	curr_dir = current.dir_entries[dir].block_num;
	
	send_message("200 OK\r\n");
	send_message("Length:0\r\n");
	send_message("\r\n");
	
	return;
}

// switch to home directory
void FileSys::home() 
{
	curr_dir = 1;
	
	send_message("200 OK\r\n");
	send_message("Length:0\r\n");
	send_message("\r\n");
}

// remove a directory
void FileSys::rmdir(const char *name)
{
	// struct to read current directory properities into
	dirblock_t current;
	// read current directory block
	bfs.read_block(curr_dir, (void *) &current);
	
	dirblock_t inner;
	
	int dir = find_directory(&current, &inner, const_cast<char*> (name));

	// error check
	if(dir == -1){
		send_message("500 file is not a directory\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}
	else if(dir == -2){
		send_message("503 File does not exist\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}
	
	// check if sub directory is empty
	if (inner.num_entries == 0) {
		
		// push up remaining items
		push_up(&current, dir);
		
		// reclaim block
		bfs.reclaim_block(current.dir_entries[dir].block_num);
		
		// delete reference in current directory 
		current.num_entries--;
		
		// write current directory back to disk
		bfs.write_block(curr_dir, (void *) &current);
		
		send_message("200 OK\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}
	
	send_message("507 Directory is not empty\r\n");
	send_message("Length:0\r\n");
	send_message("\r\n");

	return;
}

// list the contents of current directory
void FileSys::ls()
{
	int length;
	string message, list, len;
	
	// struct to read current directory properities into
	dirblock_t current, inner;

	// read current directory block
	bfs.read_block(curr_dir, (void *) &current);
	
	if (current.num_entries == 0) {
		
		// set up message
		message = "Empty Directory";
		length = message.length();
		len = "Length:" + to_string(length) + "\r\n";
		
		// send response
		send_message("200 OK\r\n");
		send_message(len);
		send_message("\r\n");
		send_message("Empty Directory\r\n");
		return;
	}
	
	message = "";
	// read names of files in directory
	for (int i = 0; i < current.num_entries; i++) {	
						
		// if not empty; determine if a directory or inode by reading block 
		// and reading magic number of block; can use same buffer as above.
		bfs.read_block(current.dir_entries[i].block_num, (void *) &inner);
		
		if (inner.magic == DIR_MAGIC_NUM) { // is directory
			message = message + strcat(current.dir_entries[i].name, "/") + " ";
		} 
		else { //is inode
			message = message + current.dir_entries[i].name + " ";
		}			
	}
	
	length = message.length();
	len = "Length:" + to_string(length) + "\r\n";
	message = message + "\r\n";
	
	send_message("200 OK\r\n");
	send_message(len);
	send_message("\r\n");
	send_message(message);
	
	return;
}

// create an empty data file
void FileSys::create(const char *name)
{
	// Need to: check name validity
	//			check for space in system
	// 			check for space in directory
	// 			create inode
	// 			link to current directory
	
	// cast to dirblock_t*
	dirblock_t current;
	// read current directory block
	bfs.read_block(curr_dir, (void *) &current);
			
	// check for name length
	if (strlen(name) > MAX_FNAME_SIZE) {
		send_message("504 File name is too long\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}
	
	// TODO: function checking naming converntions
	for(int i = 0; i < current.num_entries; i++) {
		if (strcmp(name, current.dir_entries[i].name) == 0) {
			send_message("502 File Exists\r\n");
			send_message("Length:0\r\n");
			send_message("\r\n");
			return;
		}
	}
	
	// get a free block
	short new_block = bfs.get_free_block();
	
	if (new_block == 0) {
		send_message("505 Disk is full\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}
	
	short new_entry = get_dir_entry(&current);
	
	// get next available directory entry and initalize if true
	if (new_entry >= 0) {
		current.dir_entries[new_entry].block_num = new_block;
		current.num_entries++;
		strcpy(current.dir_entries[new_entry].name, name);
	}
	else {
		send_message("506 Directory is full\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}
	
	// initialize the new inode: maybe make own function
	struct inode_t new_inode_block;
	new_inode_block.magic = INODE_MAGIC_NUM;
	new_inode_block.size = 0;
	for (int i = 0; i < MAX_DATA_BLOCKS; i++) {
		new_inode_block.blocks[i] = 0;
	}
	
	// write updated blocks to disk
	bfs.write_block(curr_dir, (void *) &current);
	bfs.write_block(new_block, (void *) &new_inode_block);
	
	send_message("200 OK\r\n");
	send_message("Length:0\r\n");
	send_message("\r\n");
	
	return;
}

// append data to a data file
void FileSys::append(const char *name, const char *data)
{

	// init
	int str_length, inode_index;
	
	// set up
	str_length = strlen(data);
	
	// first check if data to write is too long for system
	if (str_length > MAX_FILE_SIZE) {
		send_message("508 Append exceeds maximum file size\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}	
	
	// init structs
	dirblock_t current;
	inode_t inner;
	
	// read current directory block
	bfs.read_block(curr_dir, (void *) &current);

	inode_index = find_append_file(&current, &inner, const_cast<char *> (name));
	
	if(inode_index == -2){
		send_message("503 File does not exist\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}
	else if (inode_index == -1) {
		send_message("501 File is a directory\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}
		
	// now check current size
	int block_index, new_block_num, blocks_needed, count, data_index, index;

	count = -1;
	index = 0;
	
	// cases: size is 0; size is inbetween
	if (inner.size == 0) {

		// determine number of blocks needed
		blocks_needed = 1 + (str_length / BLOCK_SIZE);
		
		// initialize the datablock_t array
		datablock_t* array = new datablock_t[blocks_needed];
		
		// initialize first block needed
		new_block_num = bfs.get_free_block();
		
		if (new_block_num == 0) {
			send_message("505 Disk is full\r\n");
			send_message("Length:0\r\n");
			send_message("\r\n");
			return;
		}
		
		data_index = 0;
		inner.blocks[data_index] = new_block_num;

		for (int character = 0; character < str_length; character++) {

			// if its time for a new block
			if (character % BLOCK_SIZE == 0) {

				if (count != -1) {
					// write this last block back to disk
					bfs.write_block(new_block_num, (void *) &array[index]);
					bfs.write_block(curr_dir, (void *) &current);
					bfs.write_block(current.dir_entries[inode_index].block_num, (void *) &inner);
				
					// update block array idnex
					index++;
					
					// get next free block and entry
					new_block_num = bfs.get_free_block();
					
					if (new_block_num == 0) {
						send_message("505 Disk is full\r\n");
						send_message("Length:0\r\n");
						send_message("\r\n");
						return;
					}
					
					data_index++;
					inner.blocks[data_index] = new_block_num;	
					
					// reset count for new block
					count = 0;
				}
				else {
					count = 0;
				}
			}
			
			// write and update count
			array[index].data[count] = data[character];
			count++;
		}
		
		// write this last block back to disk
		inner.size = str_length;
		bfs.write_block(new_block_num, (void *) &array[blocks_needed - 1]);
		bfs.write_block(curr_dir, (void *) &current);
		bfs.write_block(current.dir_entries[inode_index].block_num, (void *) &inner);
	}
	else {

		// init
		int remaining, last_block, last_position, count;
		datablock_t quick;

		// check if file + new string is too much
		if (inner.size + str_length > MAX_FILE_SIZE) {
			send_message("508 Append exceeds maximum file size\r\n");
			send_message("Length:0\r\n");
			send_message("\r\n");
			return;
		}
		
		// find the current block of the tail of the file
		data_index = inner.size / BLOCK_SIZE;
		last_block = inner.blocks[data_index];
		last_position = inner.size % BLOCK_SIZE;
		
		// if we can write then check how many new blocks we will need.
		if (last_position + str_length <  BLOCK_SIZE) {
			
			// new string will fit on currently allocated block; quickly write
			blocks_needed = 0;
			bfs.read_block(last_block, (void *) &quick);
			
			for (int character = last_position; character < (last_position + str_length); character++) {
				quick.data[character] = data[character - last_position];
			}
			
			// write everything back to disk
			inner.size = inner.size + str_length;
			bfs.write_block(last_block, (void *) &quick);
			bfs.write_block(curr_dir, (void *) &current);
			bfs.write_block(current.dir_entries[inode_index].block_num, (void *) &inner);
		}
		else {

			// compute remaining space and blocks needed
			remaining = str_length - (inner.size % BLOCK_SIZE);
			blocks_needed = 1 + (remaining / BLOCK_SIZE);
			
			// initialize the datablock_t array
			datablock_t* array = new datablock_t[blocks_needed];
			
			// read the current block into array[0]
			bfs.read_block(last_block, (void *) &array[0]);
			
			if (new_block_num == 0) {
				send_message("505 Disk is full\r\n");
				send_message("Length:0\r\n");
				send_message("\r\n");
				return;
			}
			
			count = last_position;
			index = 0;
			
			for (int character = 0; character < str_length; character++) {

				// if its time for a new block
				if (count % BLOCK_SIZE == 0) {

					// write this last block back to disk
					bfs.write_block(last_block, (void *) &array[index]);
					bfs.write_block(curr_dir, (void *) &current);
					bfs.write_block(current.dir_entries[inode_index].block_num, (void *) &inner);
				
					// update block array idnex
					index++;

					// get next free block and entry
					last_block = bfs.get_free_block();
					
					if (new_block_num == 0) {
						send_message("505 Disk is full\r\n");
						send_message("Length:0\r\n");
						send_message("\r\n");
						return;
					}
					
					data_index++;
					inner.blocks[data_index] = last_block;					

					// reset count for new block
					count = 0;
	
				}
				
				// write and update count
				array[index].data[count] = data[character];
				count++;
			}
			
			// write this last block back to disk
			inner.size = inner.size + str_length;
			bfs.write_block(last_block, (void *) &array[index]);
			bfs.write_block(curr_dir, (void *) &current);
			bfs.write_block(current.dir_entries[inode_index].block_num, (void *) &inner);			
		}	
	}
	
	send_message("200 OK\r\n");
	send_message("Length:0\r\n");
	send_message("\r\n");
	return;	
}

// display the contents of a data file
void FileSys::cat(const char *name)
{
	// init
	int file_index, data_block_index, char_index;
	
	// struct to read current directory properities into
	dirblock_t current;
	inode_t inner;
	datablock_t new_block;
	
	// read current directory block
	bfs.read_block(curr_dir, (void *) &current);

	// find file index
	file_index = find_file(&current, &inner, const_cast<char*> (name));
	
	if(file_index == -1){
		send_message("501 File is a directory\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}
	else if(file_index == -2){
		send_message("503 File does not exist\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}
	
	// time to print	
	// first index will always be inode index 0
	data_block_index = 0;
	char_index = 0;
	string message, len;
	message = "";
	int length;
	for (int character = 0; character < inner.size; character++) {
		if (character % BLOCK_SIZE == 0) {
			bfs.read_block(inner.blocks[data_block_index], (void *) &new_block);
			data_block_index++;
			char_index = 0;
		}
		
		message = message + new_block.data[char_index];
		char_index++;
	}
	
	//setup
	length = message.length();
	len = "Length:" + to_string(length) + "\r\n";
	message = message + "\r\n";
	// send
	send_message("200 OK\r\n");
	send_message(len);
	send_message("\r\n");
	send_message(message);
}

// display the first N bytes of the file
void FileSys::head(const char *name, unsigned int n)
{
		// init
	int file_index, data_block_index, char_index;
	
	// struct to read current directory properities into
	dirblock_t current;
	inode_t inner;
	datablock_t new_block;
	
	// read current directory block
	bfs.read_block(curr_dir, (void *) &current);

	// find file index
	file_index = find_append_file(&current, &inner, const_cast<char*> (name));
	
	if(file_index == -2){
		send_message("503 File does not exist\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}
	else if (file_index == -1) {
		send_message("501 File is a directory\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}
	
	// time to print	
	// first index will always be inode index 0
	data_block_index = 0;
	char_index = 0;
	
	string message, len;
	message = "";
	int length;
	for (int character = 0; character < n; character++) {
		if (character % BLOCK_SIZE == 0) {
			bfs.read_block(inner.blocks[data_block_index], (void *) &new_block);
			data_block_index++;
			char_index = 0;
		}
		message = message + new_block.data[char_index];
		char_index++;
	}
	
	//setup
	length = message.length();
	len = "Length:" + to_string(length) + "\r\n";
	message = message + "\r\n";
	
	send_message("200 OK\r\n");
	send_message(len);
	send_message("\r\n");
	send_message(message);
}

// delete a data file
void FileSys::rm(const char *name)
{
	// init
	int file_index, num_data_blocks;
	dirblock_t current;
	inode_t inner;
	
	bfs.read_block(curr_dir, (void *) &current);
	
	file_index = find_file(&current, &inner, const_cast<char *> (name));
	
	if(file_index == -2){
		send_message("503 File does not exist\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}
	else if (file_index == -1) {
		send_message("501 File is a directory\r\n");
		send_message("Length:0\r\n");
		send_message("\r\n");
		return;
	}
	
	// find out number of data block used from file size,
	// delete them, then delete inode	
	if (inner.size > 0) {
		// one for first block, variable for rest
		num_data_blocks = 1 + inner.size / BLOCK_SIZE;
	}
	else {
		// know the file is empty, so only inode blok
		num_data_blocks = 1;
	}

	for (short i = 0; i < num_data_blocks; i++) {
		bfs.reclaim_block(inner.blocks[i]);
	}
	
	push_up(&current, file_index);
	bfs.reclaim_block(current.dir_entries[file_index].block_num);
	
	// now update current directory
	current.num_entries--;
	
	// write current directory back to block
	bfs.write_block(curr_dir, (void *) &current);
	
	send_message("200 OK\r\n");
	send_message("Length:0\r\n");
	send_message("\r\n");
}

// display stats about file or directory
void FileSys::stat(const char *name)
{
	int dir_block, inode_block;
	
	dirblock_t current, dir_inner;
	inode_t inode_inner;
	
	bfs.read_block(curr_dir, (void *) &current);
	
	dir_block = find_directory(&current, &dir_inner, const_cast<char *> (name));
	string message, len;
	
	int length;
	if (dir_block >= 0) {
		message = string("Directory name: ") + string(current.dir_entries[dir_block].name) + string("\n");
		message = message + string("Directory block: ") + to_string(current.dir_entries[dir_block].block_num) + "\r\n";
				
		// print results
		length = message.length();
		len = "Length:" + to_string(length) + "\r\n";
		
	
		send_message("200 OK\r\n");
		send_message(len);
		send_message("\r\n");
		send_message(message);

		return;
	}
	
	inode_block = find_file(&current, &inode_inner, const_cast<char *> (name));
	if (inode_block >= 0) {
		
		int num_blocks;
		// determine number of blocks
		if (inode_inner.size > 0) {
			// one for inode, one for first block, variable for rest
			num_blocks = 1 + 1 + inode_inner.size / BLOCK_SIZE;
		}
		else {
			// know the file is empty, so only inode blok
			num_blocks = 1;
		}
		
		// print results
		message = string("Inode block: ") + to_string(current.dir_entries[inode_block].block_num) + string("\n");
		message = message + string("Bytes in the file: ") + to_string(inode_inner.size) + string("\n");
		message = message + string("Number of blocks: ") +  to_string(num_blocks) + string("\n");
		message = message + string("First block: ") + to_string(inode_inner.blocks[0]) + string("\r\n");
		
		length = message.length();
		len = "Length:" + to_string(length) + "\r\n";

		send_message("200 OK\r\n");
		send_message(len);
		send_message("\r\n");
		send_message(message);
		return;
	}
	
	send_message("503 File does not exist\r\n");
	send_message("Length:0\r\n");
	send_message("\r\n");
	
	return;
}

// HELPER FUNCTIONS (optional)

// used for finding the next available dir_entry in current 
// directory
short FileSys::get_dir_entry(dirblock_t *directory) 
{
	// get a pointer in current directory to new directory
	for (short i = 0; i < directory->num_entries; i++) {
		if (directory->dir_entries[i].block_num == 0) {
			return i;
		}
	}
}

// used for finding directory
// returns -1 if name belongs to a file
// returns -2 if name dowsn't exist
int FileSys::find_directory(dirblock_t *current, dirblock_t *inner, char* name) {
	// read names of files in directory
	for (int i = 0; i < current->num_entries; i++) {				
		if (strcmp(current->dir_entries[i].name, name) == 0) { // name matches; check if directory
			
			// if name matches; determine if a directory or inode by reading block 
			// and reading magic number of block; can use same buffer as above.
			bfs.read_block(current->dir_entries[i].block_num, (void *) inner);
			
			//inner = *(static_cast<dirblock_t *>(static_cast<void *>(block)));
			if (inner->magic == DIR_MAGIC_NUM) { // is directory
				return i;
			} 
			else { //is inode
				return -1;
			}		
		} 		
	}
	
	return -2;
}

//used for finding a file
// returns -1 if name belongs to a directory
// returns -2 if name doesn't exist
int FileSys::find_file(dirblock_t *current, inode_t *inner, char* name) {
	//string name1 = string(name);
	//name1 = string(" ") + name1;
	// read names of files in directory
	for (int i = 0; i < current->num_entries; i++) {	
		if (strcmp(current->dir_entries[i].name, name) == 0) { // name matches; check if file

			// if name matches; determine if a directory or inode by reading block 
			// and reading magic number of block; can use same buffer as above.
			bfs.read_block(current->dir_entries[i].block_num, (void *) inner);

			if (inner->magic == INODE_MAGIC_NUM) { // is file
				return i;
			} 
			else { //is directory
				return -1;
			}		
		} 		
	}
	
	return -2;
}

int FileSys::find_append_file(dirblock_t *current, inode_t *inner, char* name) {
	string name1 = string(name);
	name1 = string(" ") + name1;
	// read names of files in directory
	for (int i = 0; i < current->num_entries; i++) {		
		if (strcmp(current->dir_entries[i].name, name1.c_str()) == 0) { // name matches; check if file

			// if name matches; determine if a directory or inode by reading block 
			// and reading magic number of block; can use same buffer as above.
			bfs.read_block(current->dir_entries[i].block_num, (void *) inner);

			if (inner->magic == INODE_MAGIC_NUM) { // is file
				return i;
			} 
			else { //is directory
				return -1;
			}		
		} 		
	}
	
	return -2;
}

void FileSys::push_up(dirblock_t *current, int dir) {
	for (int i = dir; i < current->num_entries - 1; i++) {
		current->dir_entries[i].block_num = current->dir_entries[i + 1].block_num;
		strcpy(current->dir_entries[i].name, current->dir_entries[i + 1].name);
	}
}

bool FileSys::send_message(string s)
{
	for (int i = 0; i < s.length(); i++)
	{
		char c = s[i];
		int bytes_sent = 0;
		int error_check;
		while (bytes_sent < sizeof(char))
		{
			error_check = send(fs_sock, (void*) & c, sizeof(char), 0);
			if (error_check == -1)
			{
				perror("Error Send Message");
			}
			bytes_sent += error_check;
		}
	}
}