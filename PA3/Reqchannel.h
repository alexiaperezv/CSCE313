
#ifndef _reqchannel_H_
#define _reqchannel_H_

#include "common.h"

class RequestChannel // Class defines everything that is common to all channels
{
	
public:
	enum Side {SERVER_SIDE, CLIENT_SIDE};
	enum Mode {READ_MODE, WRITE_MODE};

protected:

	string my_name;
	Side my_side;
	
	int wfd; // write file descriptor
	int rfd; // read file descriptor
	
	string s1, s2;

	virtual int open_ipc(string _pipe_name, int mode){;}
	
public:

	RequestChannel(const string _name, const Side _side):my_name(_name), my_side(_side){}
	virtual ~ RequestChannel(){}
	virtual int cread (void* msgbuf, int bufcapacity) = 0;
	virtual int cwrite(void *msgbuf , int msglen) = 0;

	string name()
	{
		return my_name;
	} 
};

#endif
