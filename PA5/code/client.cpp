#include "common.h"
#include "BoundedBuffer.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "FIFOreqchannel.h"
#include <thread>
#include <sys/wait.h>
#include <sys/epoll.h>

using namespace std;

FIFORequestChannel *create_new_channel(FIFORequestChannel *mainchan)
{
    char name[1024];
    MESSAGE_TYPE m = NEWCHANNEL_MSG;
    mainchan->cwrite(&m, sizeof(m));
    mainchan->cread(name, 1024);
    FIFORequestChannel *newchan = new FIFORequestChannel(name, FIFORequestChannel::CLIENT_SIDE);
    return newchan;
}

void patient_thread_function(int n, int pno, BoundedBuffer *request_buffer)
{
    //Push data messages for the requested patient into the request_buffer
    datamsg d(pno, 0.0, 1);

    double resp = 0;

    for (int i = 0; i < n; i++)
    {
        request_buffer->push((char *)&d, sizeof(datamsg));
        d.seconds += 0.004;
    }
}

void file_thread_function(string fname, BoundedBuffer *request_buffer, FIFORequestChannel *chan, int mb)
{
    //1. Create the file
    string recvfname = "recv/" + fname;

    //Need to make the file as long as the original length
    char buf[1024];
    filemsg f(0, 0);
    memcpy(buf, &f, sizeof(f));
    strcpy(buf + sizeof(f), fname.c_str());
    chan->cwrite(buf, sizeof(f) + fname.size() + 1);
    __int64_t filelength;
    chan->cread(&filelength, sizeof(filelength));

    FILE *fp = fopen(recvfname.c_str(), "w");
    fseek(fp, filelength, SEEK_SET);
    fclose(fp);

    //2. Generate all of the required file messages
    filemsg *fm = (filemsg *)buf;
    __int64_t remlen = filelength;

    while (remlen > 0)
    {
        fm->length = min(remlen, (__int64_t)mb);
        request_buffer->push(buf, sizeof(filemsg) + fname.size() + 1);
        fm->offset += fm->length;
        remlen -= fm->length;
    }
}

void worker_thread_function(FIFORequestChannel *chan, BoundedBuffer *request_buffer, HistogramCollection *hc, int mb)
{
    char buf[1024];
    double resp = 0;

    char recvbuf[mb];

    while (true)
    {
        request_buffer->pop(buf, 1024);
        MESSAGE_TYPE *m = (MESSAGE_TYPE *)buf;

        if (*m == DATA_MSG)
        {
            chan->cwrite(buf, sizeof(datamsg));
            chan->cread(&resp, sizeof(double));
            hc->update(((datamsg *)buf)->person, resp);
        }
        else if (*m == QUIT_MSG)
        {
            chan->cwrite(m, sizeof(MESSAGE_TYPE));
            delete chan;
            break;
        }
        else if (*m == FILE_MSG)
        {
            filemsg *fm = (filemsg *)buf;
            string fname = (char *)(fm + 1);
            int sz = sizeof(filemsg) + fname.size() + 1;
            chan->cwrite(buf, sz);
            chan->cread(recvbuf, mb);

            string recvfname = "recv/" + fname;

            FILE *fp = fopen(recvfname.c_str(), "r+");
            fseek(fp, fm->offset, SEEK_SET);
            fwrite(recvbuf, 1, fm->length, fp);
            fclose(fp);
        }
    }
}

void event_polling_function(int n, int p, int w, int mb, FIFORequestChannel** wchan, BoundedBuffer* request_buffer, HistogramCollection* hc)
{ 
    char buf[1024];
    double resp = 0;

    char recvbuf[mb];

    // first create empty epoll list and then add everything from there

    struct epoll_event ev;
    struct epoll_event events[w];

    // this part creates the actual emptylist

    int epollfd = epoll_create1(0); // returns file descriptor that is handle to epoll list
    if (epollfd == -1)
    {
        EXITONERROR("epoll_create1");
    }

    // mapping to be able to access the channels with data later

    unordered_map<int, int> fd_to_index; // file descriptor to index
    vector<vector<char>> state(w);       // saves messages sent to be able to gather them later

    int nsent = 0;
    int nrecv = 0;

    bool quit_recv = false;

    // priming step: send w initial requests (all channels are carrying some data)
    for (int i = 0; i < w; i++)
    {
        int sz = request_buffer->pop(buf, 1024); // take whatever is in the request buffer and "send it through"
        if (*(MESSAGE_TYPE *)buf == QUIT_MSG)
        {
            quit_recv = true;
            break;
        }
        wchan[i]->cwrite(buf, sz);

        // Record the state[i]
        state[i] = vector<char>(buf, buf + sz);
        nsent++;
        int rfd = wchan[i]->getrfd();
        fcntl(rfd, F_SETFL, O_NONBLOCK);

        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = rfd;
        fd_to_index[rfd] = i;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, rfd, &ev) == -1)
        {
            EXITONERROR("epoll_ctl: listen_sock");
        }
    }

    // at this point, nsent = w, nrecv = 0
    while (true)
    {

        if (quit_recv && nrecv == nsent)
        {
            break;
        }
        // keeps polling until there is some activity
        int nfds = epoll_wait(epollfd, events, w, -1); // tells how many of w have some data
        if (nfds == -1)
        {
            EXITONERROR("epoll_wait");
        }
        for (int i = 0; i < nfds; i++)
        {
            // checks which channel is ready to be read

            int rfd = events[i].data.fd;
            int index = fd_to_index[rfd];
            int resp_sz = wchan[index]->cread(recvbuf, mb); // this wouldn't work without mapping
            nrecv++;
            // gather the message that was initially sent
            // process (recvbuf)
            vector<char> req = state[index]; // now we know what the request was
            char *request = req.data();

            // processing response
            MESSAGE_TYPE *m = (MESSAGE_TYPE *)request;
            if (*m == DATA_MSG)
            {
                // cout << "recvd: " << *(double*)recvbuf << endl;
                hc->update(((datamsg *)request)->person, *(double *)recvbuf);
            }
            else if (*m == FILE_MSG)
            {
                filemsg *f = (filemsg *)m;
                string filename = (char *)(f + 1);
                string recvfname = "recv/" + filename;

                FILE *fp = fopen(recvfname.c_str(), "r+");
                fseek(fp, f->offset, SEEK_SET);
                fwrite(recvbuf, 1, f->length, fp);
                fclose(fp);
            }

            // reuse the channel that has already "delivered" its data
            if (!quit_recv)
            {
                int req_sz = request_buffer->pop(buf, sizeof(buf));
                if (*(MESSAGE_TYPE *)buf == QUIT_MSG)
                {
                    // only nrecv should increment in this case
                    quit_recv = true;
                }
                else
                {
                    wchan[index]->cwrite(buf, req_sz);
                    // we have to remember what we sent through, update the state
                    state[index] = vector<char>(buf, buf + req_sz);
                    nsent++;
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    int n = 15;          // default number of requests per "patient"
    int p = 10;          // number of patients [1,15]
    int w = 10;          // default number of worker threads
    int b = 100;         // default capacity of the request buffer, you should change this default
    int m = MAX_MESSAGE; // default capacity of the message buffer
    srand(time_t(NULL));
    string fname = "";

    bool p_req = false;
    bool f_req = false;
    string m_s = "";

    //Get Opt functionality to set variables
    int opt = -1;
    while ((opt = getopt(argc, argv, "m:n:b:w:p:f:")) != -1)
    {
        switch (opt)
        {
        case 'm':
            m = atoi(optarg);
            m_s = optarg;
            break;
        case 'n':
            n = atoi(optarg);
            break;
        case 'p':
            p = atoi(optarg);
            p_req = true;
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 'w':
            w = atoi(optarg); // meaning of w has changed, it is no longer the # of request channels & worker channels, now it simply represents the # of request channels
            break;
        case 'f':
            fname = optarg;
            f_req = true;
            break;
        }
    }

    if(m_s == "")
    {
        m_s = "256";
    }

    int pid = fork();
    if (pid == 0)
    {
        // modify this to pass along m
        execl("server", "server", "-m", (char*)m_s.c_str(), (char*)NULL);
    }

    FIFORequestChannel *chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
    BoundedBuffer request_buffer(b);
    HistogramCollection hc;

    //Making histograms and adding to the histogram collection hc

    for (int i = 0; i < p; i++)
    {
        Histogram *h = new Histogram(10, -2.0, 2.0);
        hc.add(h);
    }

    struct timeval start, end;
    gettimeofday(&start, 0);

    //Make w worker channels (do sequentially in the main)
    FIFORequestChannel *wchan[w];

    // START ALL THREADS HERE

    if(p_req) // if it is a patient request
    {
        for(int i = 0; i < w; i++)
        {
            wchan[i] = create_new_channel(chan);
        }

        thread patient[p];
        for(int i = 0; i < p; i++)
        {
            patient[i] = thread(patient_thread_function, n, i+1, &request_buffer);
        }

        thread evp(event_polling_function, n, p, w, m, (FIFORequestChannel**)wchan, &request_buffer, &hc);

        for(int i = 0; i < p; i++)
        {
            patient[i].join();
        }

        cout << "Patient threads have completed." << endl;

        MESSAGE_TYPE q = QUIT_MSG;
        request_buffer.push((char*)&q, sizeof(q));

        evp.join();
        cout << "Worker threads have completed." << endl;
    }

    if(f_req) // if it is a file request
    {
        for(int i = 0; i < w; i++)
        {
            wchan[i] = create_new_channel(chan);
        }

        thread file_thread(file_thread_function, fname, &request_buffer, chan, m);
        thread evp(event_polling_function, n, p, w, m, (FIFORequestChannel**)wchan, &request_buffer, &hc);

        file_thread.join();
        cout << "File threads have completed." << endl;

        MESSAGE_TYPE q = QUIT_MSG;
        request_buffer.push((char*)&q, sizeof(q));

        evp.join();
        cout << "Worker threads have completed." << endl;
    }

    gettimeofday(&end, 0);

    // print results
    hc.print();
    
    //print time diff
    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec) / (int)1e6;
    int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec) % ((int)1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

    MESSAGE_TYPE q = QUIT_MSG;
    
    for(int i = 0; i < w; i++)
    {
        wchan[i]->cwrite((char*)&q, sizeof(MESSAGE_TYPE));
        delete wchan[i];
    }
    
    cout << "ALl worker channls have been deleted." << endl;
    
    chan->cwrite((char *)&q, sizeof(MESSAGE_TYPE));
    cout << "All Done!!!" << endl;
    delete chan;

    wait(0);
}
