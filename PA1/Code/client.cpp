/*
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date  : 2/8/20
 */
#include "common.h"
#include "FIFOreqchannel.h"

using namespace std;

int main(int argc, char *argv[])
{

    // Requirements: run server as child process, request individual data points, request entire files, request new channels,
    // close all channels at the end

    // to run the server as child of client process

    int pid = fork();

    if (pid == 0)
    {
        execvp("./server", argv);
    }
    else
    {
        // this is the parent process
        FIFORequestChannel chan("control", FIFORequestChannel::CLIENT_SIDE); // create client

        // need to determine whether user wants to request a point or a data file
        // set default values

        int c = 0; // used to read flags from command line

        int patient = -1;
        double time = -1;
        int ecg = 0;
        bool newChannel = false;
        string fileName = "";

        int buffercapacity = MAX_MESSAGE; // change this in order to increase buffer capacity

        // depending on the flags entered in the command line, we determine whether client wants to request a file, data point, or new channel
        while ((c = getopt(argc, argv, "p:t:e:f:cm:")) != -1)
        { //gets arguments from command line and puts them into c
            switch (c)
            {
            case 'p': //if p flag is present
                if (optarg)
                {
                    patient = atoi(optarg);
                } //set patient to value of p flag
                break;
            case 't':
                if (optarg)
                { // if t flag is present
                    // we round the argument to nearest 0.004 seconds
                    time = atof(optarg); // set time to value of t flag
                }
                break;
            case 'e': // if e flag is present
                if (optarg)
                {
                    ecg = atoi(optarg);
                }
                break;
            case 'f': // in this case the user is wanting to request a whole file
                if (optarg)
                {
                    fileName = string(optarg);
                }
                break;
            case 'c':
                newChannel = true; //set new channel boolean to true
                break;
            case '?':
                EXITONERROR("Invalid Option");
                break;
            case 'm':
                cout << "Changing initiatl buffer capacity of: " << buffercapacity << "..." << endl;
                buffercapacity = atoi(optarg);
                cout << "DONE! New buffer capacity is: " << buffercapacity << endl;
                break;
            }
        }

        //once we have processed the command line message, we must make sure arguments are valid
        if ((time < 0 || time > 59.996) && time != -1)
        {
            EXITONERROR("Invalid time");
        }
        if (ecg < 0 || ecg > 2)
        {
            EXITONERROR("Invalid ecg value");
        }

        // we now determine whether the client wants to request a data point or a whole file
        if (time != -1 && ecg != 0)
        {
            if (patient < 1 || patient > 15)
            {
                EXITONERROR("Invalid patient");
            }
            // if the patient, ecg ,and time values are valid, we request the data point from server
            char buffer[buffercapacity];
            datamsg *dataPoint = new datamsg(patient, time, ecg); // data message constructor
            chan.cwrite(dataPoint, sizeof(datamsg));
        
            double *result = new double;
            chan.cread(result, sizeof(double));
            cout << "At time " << time << " patient " << patient << "'s ecg number " << ecg << " was: " << *result << endl;

            delete result;
            delete dataPoint;
        }

        // if they do not want an individual data point, instead they want multiple points:
        else if (patient != -1)
        {
            if (patient < 1 || patient > 15)
            {
                EXITONERROR("Invalid patient");
            }

            struct timeval start; // used to calculate how long the process takes using this approach
            gettimeofday(&start, NULL);
            double totalTime = 0;
            // iterate through the data message and send to a new file
            ofstream myfile;
            string fileName = "x" + to_string(patient) + ".csv";
            myfile.open(fileName);

            // iterate through first 1500 data points in file
            for (int i = 0; i < 1500; i++)
            {

                //first ecg column
                //char buffer[buffercapacity];
                datamsg *send = new datamsg(patient, totalTime, 1);
                chan.cwrite(send, sizeof(datamsg));

                double *received = new double;
                chan.cread(received, sizeof(double));
                myfile << totalTime << "," << *received << ",";

                //second ecg column
                send = new datamsg(patient, totalTime, 2);
                chan.cwrite(send, sizeof(datamsg));
                chan.cread(received, sizeof(double));
                myfile << *received << endl;
                totalTime += 0.004;

                delete send;
                delete received;
            }

            struct timeval end;
            gettimeofday(&end, NULL);

            // Now we need to convert the time to milliseconds and find the difference:
            double totalStart = 0;
            double totalEnd = 0;
            totalStart = (double)start.tv_usec + (double)start.tv_sec * 1000000;
            totalEnd = (double)end.tv_usec + (double)end.tv_sec * 1000000;
            cout << "The data exchange performed took: " << totalEnd - totalStart << " microseconds." << endl;
        }

        // Next we handle the data transferring of full files
        else if (fileName != "")
        {
            // we first need to know the lenght of the file:
            filemsg getLen(0, 0);
            char *buff = new char[sizeof(filemsg) + fileName.size() + 1];
            memcpy(buff, &getLen, sizeof(filemsg));
            memcpy(buff + sizeof(filemsg), fileName.c_str(), fileName.size() + 1);
            chan.cwrite(buff, sizeof(filemsg) + fileName.size() + 1);
            __int64_t filelen;
            chan.cread(&filelen, sizeof(__int64_t));
            cout << "File lenght: " << filelen << endl;

            //define the output file
            ofstream myfile;
            string file_name = "received/" + fileName;
            myfile.open(file_name, ios::out | ios::binary); // make sure output file is binary
            //myfile.open(file_name, ios::binary);
            struct timeval start;
            gettimeofday(&start, NULL); // starts the timer

            //now we send the message to get the entire file, divide in 256 byte-pieces
            __int64_t offset = 0;
            //int variableLength = 0;
            int length = filelen;
            while (length > offset)
            {
                if (length - offset < buffercapacity)
                {
                    //cout << "Test100" << endl;
                    // there is less data than the usual 256 bytes we collect
                    filemsg *send = new filemsg(offset, length - offset);
                    //cout << "Test101" << endl;
                    char *newBuff = new char[sizeof(filemsg) + fileName.size() + 1];
                    memcpy(newBuff, send, sizeof(filemsg));
                    memcpy(newBuff + sizeof(filemsg), fileName.c_str(), fileName.size() + 1);
                    //cout << "Test102" << endl;
                    chan.cwrite(newBuff, sizeof(filemsg) + fileName.size() + 1);
                    //cout << "Test103" << endl;
                    char *received = new char [buffercapacity];
                    chan.cread(received, buffercapacity);
                    //cout << "Test104" << endl;
                    myfile.write(received, length - offset);
                    //cout << "Test105" << endl;
                    offset += length - offset;

                    delete newBuff;
                    delete send;
                    delete received;
                }
                else
                {
                    //cout << "Test200" << endl;
                    filemsg *send = new filemsg(offset, buffercapacity);
                    //cout << "Test201" << endl;
                    char *newBuff = new char[sizeof(filemsg) + fileName.size() + 1];
                    memcpy(newBuff, send, sizeof(filemsg));
                    memcpy(newBuff + sizeof(filemsg), fileName.c_str(), fileName.size() + 1);
                    //cout << "Test202" << endl;
                    chan.cwrite(newBuff, sizeof(filemsg) + fileName.size() + 1);
                    //cout << "Test203" << endl;
                    char *received = new char [buffercapacity];
                    chan.cread(received, buffercapacity);
                    //cout << "Test204" << endl;
                    myfile.write(received, buffercapacity);
                    //cout << "Test205" << endl;
                    offset += buffercapacity;

                    delete send;
                    delete received;
                    delete newBuff;
                }
            }
            delete buff;
            // calculate how much time file exchange took
            struct timeval end;
            gettimeofday(&end, NULL);

            double totalStart = 0;
            double totalEnd = 0;
            totalStart = (double)start.tv_usec + (double)start.tv_sec * 1000000;
            totalEnd = (double)end.tv_usec + (double)end.tv_sec * 1000000;

            cout << "The data exchange performed took: " << totalEnd - totalStart << " microseconds" << endl;
        }
        else if (newChannel)
        {
            //cout << "New Channel" << endl;
            //if user requested a new channel
            MESSAGE_TYPE n = NEWCHANNEL_MSG;
            chan.cwrite(&n, sizeof(MESSAGE_TYPE));
            //cout << "Test1" << endl;

            char *newChan = new char [30];
            chan.cread(newChan, buffercapacity);
            //cout << "C: " << newChan << endl;
            FIFORequestChannel newChannel(newChan, FIFORequestChannel::CLIENT_SIDE);
            //cout << "Test2" << endl;

            // test to make sure that new channel can receive requests/send data
            char buffer[buffercapacity];
            datamsg *testMessage = new datamsg(5, 0.32, 1);
            newChannel.cwrite(testMessage, sizeof(datamsg));
            //cout << "Test3" << endl;

            double *received = new double;
            newChannel.cread(received, sizeof(double));
            cout << "The ecg 1 value for person 5 at time 0.32 was: " << *received << endl;

            delete received;
            delete newChan;
            delete testMessage;

            //close the channel
            MESSAGE_TYPE close = QUIT_MSG;
            newChannel.cwrite(&close, sizeof(MESSAGE_TYPE));

            //cout << "New Channel End" << endl;
        }
        // closing the channel
        MESSAGE_TYPE m = QUIT_MSG;
        chan.cwrite(&m, sizeof(MESSAGE_TYPE));
        // wait on child process
        // wait(NULL);
        usleep(1000000);
    }
}