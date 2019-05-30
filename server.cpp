// Authors: Jack Gularte and Tony Caballero
// Date: March 11th, 2019
// Filename: server.cpp
// For: Prof. Zhu; CPSC 3500
// Compiles with: make
// Runs with: ./nfsserver $port_number

#include <iostream>
#include <string>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <vector> 

#include "FileSys.h" 

using namespace std;

const int BACKLOG = 1; // max length


bool receive_message(string &msg, int connection);

int main(int argc, char* argv[]) {
	if (argc < 2) {
		cout << "Usage: ./nfsserver port#\n";
        return -1;
    }

    
    int port = atoi(argv[1]);

    //networking part: create the socket and accept the client connection

    //socket 
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        cerr <<" Socket failure. " << endl;
        return -1;
    }

    //create struct address
    sockaddr_in serveraddress;
    serveraddress.sin_family = AF_INET;
    serveraddress.sin_port = htons(port); 
    serveraddress.sin_addr.s_addr = htonl(INADDR_ANY); 

	// bind
    if (bind (sockfd,(sockaddr*) &serveraddress, sizeof(serveraddress)) == -1)
    {
        cerr << " Bind failure. " << endl;
        return -1;
    }

	// listen
    cout <<" Server listening on port: " << port << endl;
    listen(sockfd, BACKLOG);
	
	// accept
    int connection = accept(sockfd, NULL, 0); 
	close(sockfd);

	
	// mount and take command
    FileSys fs; 
    fs.mount(connection); 
                   
    cout <<" Server and client is connected. " << endl;

    string msg, level_one, level_two, data, space, value, intermediate;
    while(true)
    {
        string response;
        if (!receive_message(response, connection))
        {
            break;
        }
      
        string mMsg = response.substr(0, response.length() - 2);
        string cmd = mMsg.substr(0, mMsg.find(" "));
     
		if (cmd == "mkdir")
		{
			string dir = mMsg.substr(5);
			fs.mkdir(dir.c_str());

		}
		else if (cmd == "cd")
		{
			string dir = mMsg.substr(2);
			fs.cd(dir.c_str());
		}

		else if (cmd == "home")
		{
			fs.home();
		}

		else if (cmd == "rmdir")
		{
			string dir = mMsg.substr(5);
			fs.rmdir(dir.c_str());
		}

		else if (cmd == "ls")
		{
			fs.ls();
		}

		else if (cmd == "create")
		{
			string dir = mMsg.substr(6);
			fs.create(dir.c_str());
		}

		else if (cmd == "append")
		{
			// kinda gross
			level_one = mMsg.substr(7);
			level_two = level_one.substr(0, level_one.find(" "));
			data = level_one.substr(level_two.length() + 1);
			fs.append(level_two.c_str(), data.c_str());
		}

		else if (cmd == "cat")
		{
			string dir = mMsg.substr(3);
			fs.cat(dir.c_str());
		}

		else if (cmd == "head")
		{
			// gross
			space = mMsg.substr(0, mMsg.find("\r\n"));
			level_one = space.substr(4);
			value = level_one.substr(1, level_one.find(" "));
			intermediate = level_one.substr(value.length()+1);
			level_two = intermediate.substr(0, intermediate.find(" "));
			data = intermediate.substr(level_two.length() + 1);		   
			fs.head(level_two.c_str(), stoi(data));
		}

		else if (cmd == "rm")
		{
			string dir = mMsg.substr(2);
			fs.rm(dir.c_str());
		}

		else 
		{
			string dir = mMsg.substr(4);
			fs.stat(dir.c_str());
		}
	}
	
    //close the listening socket
	close(sockfd);
    //unmout the file system
    fs.unmount();
	
    return 0;
}



// Used for receiving data bit by bit
bool receive_message(string &msg, int connection)
{
  vector<char> line;
  char charac;
  bool finished = false;
  bool slash_r_last = false;
  while(!finished)
  {
    int bytes_received = 0;
    int error_check;
    while (bytes_received < sizeof(char))
    {
        error_check = recv(connection, (void*) & charac, sizeof(char), 0);
        if (error_check == -1)
        {
            return false;
        }
    
        bytes_received += error_check;
    }
    if (charac == '\n' && slash_r_last)
	{
		finished = true;
	}
      slash_r_last = (charac == '\r');
      line.push_back(charac);
  
  }
    msg = string(line.begin(), line.end());
    return true;
}