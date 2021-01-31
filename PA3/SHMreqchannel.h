
#ifndef _SHMreqchannel_H_
#define _SHMreqchannel_H_

#include "common.h"
#include "Reqchannel.h"

#include <semaphore.h>
#include <sys/mman.h>
#include <string>
using namespace std;

class SHMQ
{
private: 

	char* segment;
	sem_t* sd;
	sem_t* rd;
	string name;
	int len;

public: 
	SHMQ (string _name, int _len) : name (_name), len (_len)
	{
		int fd = shm_open(name.c_str(), O_RDWR | O_CREAT, 0600);
		if(fd < 0)
		{
			EXITONERROR("could not create/open shared memory segment");
			exit(0);
		}
		ftruncate(fd, len); // set the length to 1024, the default is 0, so this is 

		segment = (char* )mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
		if(!segment)
		{
			EXITONERROR("Cannot map");
		}
		rd = sem_open((name + "_rd").c_str(), O_CREAT, 0600, 1); // initial value 1
		sd = sem_open((name + "_sd").c_str(), O_CREAT,0600,0); // initial value 0
	}

	int my_shm_send(void * msg, int len)
	{
		sem_wait(rd);
		memcpy(segment, msg, len);
		sem_post(sd);
		return len;
	}
	int my_shm_recv(void * msg, int len)
	{
		sem_wait(sd);
		memcpy(msg, segment, len);
		sem_post(rd);
		return len;
	}
	~SHMQ()
	{
		sem_close(sd);
		sem_close(rd);
		sem_unlink((name + "_rd").c_str());
		sem_unlink((name + "_sd").c_str());

		munmap(segment, len);
		shm_unlink(name.c_str());
	}
};

// We will use 2 semaphores for shared memory request channel
class SHMRequestChannel : public RequestChannel // Class defines Shared Memory request channel
{
private:
	SHMQ* shmq1;
	SHMQ* shmq2;
	int len;
public:
	SHMRequestChannel(const string _name, const Side _side, int _len);
	/* Creates a "local copy" of the channel specified by the given name. 
	 If the channel does not exist, the associated IPC mechanisms are 
	 created. If the channel exists already, this object is associated with the channel.
	 The channel has two ends, which are conveniently called "SERVER_SIDE" and "CLIENT_SIDE".
	 If two processes connect through a channel, one has to connect on the server side 
	 and the other on the client side. Otherwise the results are unpredictable.

	 NOTE: If the creation of the request channel fails (typically happens when too many
	 request channels are being created) and error message is displayed, and the program
	 unceremoniously exits.

	 NOTE: It is easy to open too many request channels in parallel. Most systems
	 limit the number of open files per process.
	*/

	~SHMRequestChannel();
	/* Destructor of the local copy of the bus. By default, the Server Side deletes any IPC 
	 mechanisms associated with the channel. */

	int cread(void *msgbuf, int bufcapacity);
	/* Blocking read of data from the channel. You must provide the address to properly allocated
	memory buffer and its capacity as arguments. The 2nd argument is needed because the recepient 
	side may not have as much capacity as the sender wants to send.
	
	In reply, the function puts the read data in the buffer and  
	returns an integer that tells how much data is read. If the read fails, it returns -1. */

	int cwrite(void *msgbuf, int msglen);
	/* Writes msglen bytes from the msgbuf to the channel. The function returns the actual number of 
	bytes written and that can be less than msglen (even 0) probably due to buffer limitation (e.g., the recepient
	cannot accept msglen bytes due to its own buffer capacity. */

};

#endif
