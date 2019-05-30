// Authors: Jack Gularte and Anthony Caballero
// For: Prof. Zhu; CPSC 3500
// Date: March 11 2019
// File: Shell.cpp
// Compiles With: make
// Runs With: ./nfsclient $ip_address:$port_number


#include <iostream>
#include <fstream>
#include <sstream>
#include <string.h>
#include <stdio.h> 
#include <vector> 

using namespace std;

#include "Shell.h"

const int APPEND_NUM = 0;
const int CAT_NUM = 1;
const int CD_NUM = 2;
const int CREATE_NUM = 3;
const int HOME_NUM = 4;
const int LS_NUM = 5;
const int MKDIR_NUM = 6;
const int RM_NUM = 7;
const int RMDIR_NUM = 8;
const int STAT_NUM = 9;
const int EXIT_NUM = 10;

static const string PROMPT = "Jack/Tony NFS> ";

// Mount the network file system with server name and port number in the 
//format of server:port
void Shell::mountNFS(string fs_loc) 
{
  
	if (is_mounted)
	{
		cout <<" Already mounted. " << endl;
		return;
	}

	string port, server;
	stringstream stream(fs_loc);
	getline(stream, server,':');
	getline(stream, port, '\0');
	cout << "Server Name: " << server << " Port: " << port << endl;

	addrinfo* addr, hints; 

	hints.ai_family = AF_INET; 
	hints.ai_socktype = SOCK_STREAM; 

	//determine if getaddressinfo is successful
	if (getaddrinfo(server.c_str(), port.c_str(), &hints, &addr) !=0)
	{
		cerr <<" Get server info failed. " << endl;
		return;
	}


	//create a client socket using ip4 and TCP
	cs_sock = socket(AF_INET, SOCK_STREAM, 0); 
	if (cs_sock == -1)
	{
		cerr <<" Client socket creation failed. " << endl;
		cout << " Client socket creation failed." << endl;
		freeaddrinfo(addr); //free dynamic memory in addr
		return;
	}

	//determine if successful
	if (connect (cs_sock, addr->ai_addr, addr->ai_addrlen) == -1)
	{
		cerr << " Connection to server failed. " << endl;
		freeaddrinfo(addr); 
		return;
	}

	//if all the above operations are completed successfully, 
	freeaddrinfo(addr); 
	is_mounted = true; 
	cout <<" Connection to server is established. " << endl;
}

void Shell::unmountNFS() 
{
	// close the socket if it was mounted
  if (!is_mounted)
  {
    cout <<" Server has not been mounted; cannot unmount " << endl;
    return;
  }
  
  close (cs_sock); //close the socket

}

// Remote procedure call on mkdir
void Shell::mkdir_rpc(string dname) 
{
	CommandSend("mkdir " + dname + "\r\n");
	string response;
	
	// code
	receive(response);

	// length
	if(strcmp(response.c_str(), "200 OK\r\n") == 0){
		receive(response);
		receive(response);
		cout << "Success\n";
		return;
	} 
	else {
		cout << response;
		receive(response);
		receive(response);
	}

}

// Remote procedure call on cd
void Shell::cd_rpc(string dname) 
{
	CommandSend("cd " + dname + "\r\n");
	string response;
	
	// code
	receive(response);
	
	// length
	if(strcmp(response.c_str(), "200 OK\r\n") == 0){
		receive(response);
		receive(response);
		cout << "Success\n";
		return;
	} 
	else {
		cout << response;
		receive(response);
		receive(response);
	}
}

// Remote procedure call on home
void Shell::home_rpc() 
{
	CommandSend("home\r\n");
	string response;
	// code
	receive(response);
	
	// length
	if(strcmp(response.c_str(), "200 OK\r\n") == 0){
		receive(response);
		receive(response);
		cout << "Success\n";
		return;
	} 
	else {
		cout << response;
		receive(response);
		receive(response);
	}
}

// Remote procedure call on rmdir
void Shell::rmdir_rpc(string dname) 
{
	CommandSend("rmdir " + dname +"\r\n");
	string response;
	
	// code
	receive(response);
	
	// length
	if(strcmp(response.c_str(), "200 OK\r\n") == 0){
		receive(response);
		receive(response);
		cout << "Success\n";
		return;
	} 
	else {
		cout << response;
		receive(response);
		receive(response);
	}
}

// Remote procedure call on ls
void Shell::ls_rpc() 
{
	CommandSend("ls\r\n");
	string response;

	receive(response);
	
	// length
	if(strcmp(response.c_str(), "200 OK\r\n") == 0){
		receive(response);
		receive(response);
		receive(response);
		cout << response;
		return;
	} 
	else {
		cout << response;
		receive(response);
		receive(response);
	}
}

// Remote procedure call on create
void Shell::create_rpc(string fname) 
{
	CommandSend("create " + fname+ "\r\n");
	string response;
	
	// code
	receive(response);
	
	// length
	if(strcmp(response.c_str(), "200 OK\r\n") == 0){
		receive(response);
		receive(response);
		cout << "Success\n";
		return;
	} 
	else {
		cout << response;
		receive(response);
		receive(response);
	}
}

// Remote procedure call on append
void Shell::append_rpc(string fname, string data) 
{
	CommandSend("append " + fname + " " + data + "\r\n");
	string response;
	
	// code
	receive(response);
	
	// length
	if(strcmp(response.c_str(), "200 OK\r\n") == 0){
		receive(response);
		receive(response);
		cout << "Success\n";
		return;
	} 
	else {
		cout << response;
		receive(response);
		receive(response);
	}
}

// Remote procesure call on cat
void Shell::cat_rpc(string fname) 
{
	string response;
	CommandSend("cat " + fname + "\r\n");
	
	// get code
	receive(response);

	if(strcmp(response.c_str(), "200 OK\r\n") == 0){
		receive(response);
		receive(response);
		receive(response);
		cout << response;
		return;
	} else {	
		cout << response;
		receive(response);
		receive(response);
		receive(response);
	}
}

// Remote procedure call on head
void Shell::head_rpc(string fname, int n) 
{
	CommandSend("head " + fname + " " + to_string(n) + "\r\n");
	string response;
	
	receive(response);
	
	if(strcmp(response.c_str(), "200 OK\r\n") == 0){
		receive(response);
		receive(response);
		receive(response);
		cout << response;
	}
	else {
		cout << response;
		receive(response);
		receive(response);
		receive(response);
	}
}

// Remote procedure call on rm
void Shell::rm_rpc(string fname) 
{
	string response;
	CommandSend("rm " + fname + "\r\n");
  
	// code
	receive(response);
	
	// length
	if(strcmp(response.c_str(), "200 OK\r\n") == 0){
		receive(response);
		receive(response);
		cout << "Success\n";
		return;
	} 
	else {
		cout << response;
		receive(response);
		receive(response);
	}
}

// Remote procedure call on stat
void Shell::stat_rpc(string fname) 
{
  string response;
  CommandSend("stat " + fname + "\r\n");
  
  receive(response);

  if(strcmp(response.c_str(), "200 OK\r\n") == 0){
    receive(response);
    receive(response);
	receive(response);
    cout << response;
  }else{
    cout << response;
	receive(response);
	receive(response);
	receive(response);
  }
}

// Executes the shell until the user quits.
void Shell::run()
{
  // make sure that the file system is mounted
  if (!is_mounted)
 	return; 
  
  // continue until the user quits
  bool user_quit = false;
  while (!user_quit) {

    // print prompt and get command line
    string command_str;
    cout << PROMPT;
    getline(cin, command_str);

    // execute the command
    user_quit = execute_command(command_str);
  }

  // unmount the file system
  unmountNFS();
}

// Execute a script.
void Shell::run_script(char *file_name)
{
  // make sure that the file system is mounted
  if (!is_mounted)
  	return;
  // open script file
  ifstream infile;
  infile.open(file_name);
  if (infile.fail()) {
    cerr << "Could not open script file" << endl;
    return;
  }


  // execute each line in the script unless the user type "quit"
  bool user_quit = false;
  string command_str;
  getline(infile, command_str, '\n');
  while (!infile.eof() && !user_quit) {
    cout << PROMPT << command_str << endl;
    user_quit = execute_command(command_str);
    getline(infile, command_str);
  }

  // clean up
  unmountNFS();
  infile.close();
}


// Executes the command. Returns true for quit and false otherwise.
bool Shell::execute_command(string command_str)
{
  // parse the command line
  struct Command command = parse_command(command_str);

  // look for the matching command
  if (command.name == "") {
    return false;
  }
  else if (command.name == "mkdir") {
    mkdir_rpc(command.file_name);
  }
  else if (command.name == "cd") {
    cd_rpc(command.file_name);
  }
  else if (command.name == "home") {
    home_rpc();
  }
  else if (command.name == "rmdir") {
    rmdir_rpc(command.file_name);
  }
  else if (command.name == "ls") {
    ls_rpc();
  }
  else if (command.name == "create") {
    create_rpc(command.file_name);
  }
  else if (command.name == "append") {
    append_rpc(command.file_name, command.append_data);
  }
  else if (command.name == "cat") {
    cat_rpc(command.file_name);
  }
  else if (command.name == "head") {
    errno = 0;
    unsigned long n = strtoul(command.append_data.c_str(), NULL, 0);
    if (0 == errno) {
      head_rpc(command.file_name, n);
    } else {
      cerr << "Invalid command line: " << command.append_data;
      cerr << " is not a valid number of bytes" << endl;
      return false;
    }
  }
  else if (command.name == "rm") {
    rm_rpc(command.file_name);
  }
  else if (command.name == "stat") {
    stat_rpc(command.file_name);
  }
  else if (command.name == "quit") {
    return true;
  }

  return false;
}

// Parses a command line into a command struct. Returned name is blank
// for invalid command lines.
Shell::Command Shell::parse_command(string command_str)
{
  // empty command struct returned for errors
  struct Command empty = {"", "", ""};

  // grab each of the tokens (if they exist)
  struct Command command;
  istringstream ss(command_str);
  int num_tokens = 0;
  if (ss >> command.name) {
    num_tokens++;
    if (ss >> command.file_name) {
      num_tokens++;
      if (ss >> command.append_data) {
        num_tokens++;
        string junk;
        if (ss >> junk) {
          num_tokens++;
        }
      }
    }
  }

  // Check for empty command line
  if (num_tokens == 0) {
    return empty;
  }
    
  // Check for invalid command lines
  if (command.name == "ls" ||
      command.name == "home" ||
      command.name == "quit")
  {
    if (num_tokens != 1) {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else if (command.name == "mkdir" ||
      command.name == "cd"    ||
      command.name == "rmdir" ||
      command.name == "create"||
      command.name == "cat"   ||
      command.name == "rm"    ||
      command.name == "stat")
  {
    if (num_tokens != 2) {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else if (command.name == "append" || command.name == "head")
  {
    if (num_tokens != 3) {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else {
    cerr << "Invalid command line: " << command.name;
    cerr << " is not a command" << endl; 
    return empty;
  } 

  return command;
}



 bool Shell::CommandSend(string s)
 {
	int err = 0;
	int count;
	int size = s.length();
   for (int i = 0; i < size; i++)
   {
     char buff = s[i];
     count = 0;
     while (count < sizeof(char))
     {
       if((err = send(cs_sock, (void*)&buff, sizeof(char), 0)) == -1){
       //if (error_check == -1)
		   //return true;
         perror("Error Send Message");
	   }
       count += err;
     }
   }
 }

//receive data
bool Shell::receive(string &msg)
{
	vector<char> vec;
	char buff;
	bool finished = false;
	bool last = false;
	int err;
	int count;
	while(!finished)
	{
		count = 0;
		while (count < sizeof(char))
		{
			if((err = recv(cs_sock, (void*)&buff, sizeof(char), 0)) == -1)
			{
				return false;
			}

			count += err;
		}
		if (buff == '\n' && last)
		  {
			finished = true;
		  }
		  
		  last = (buff == '\r');
		  vec.push_back(buff);
		}
	
	msg = string(vec.begin(), vec.end());
	return true;
}




