#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <vector>
#include <string>
#include <cstring>

using namespace std;

string trim(string input)
{
    string trimmed;
    size_t first = input.find_first_not_of(" ");
    size_t last = input.find_last_not_of(" ");
    input = input.substr(first);
    trimmed = input.substr(0, last + 1);
    // so far, the whitespace at the beginning and end has been removed
    // now we must remove quotation marks "if found"
    if (trimmed.find("\"") != string::npos)
    {
        // if we find quotation marks
        string trimmed2;
        size_t first = trimmed.find_first_of("\""); // find index of first quotation mark
        if (first > 0)
        {
            trimmed2 = trimmed.substr(0, first);
        }
        trimmed = trimmed.substr(first + 1);
        size_t second = trimmed.find_last_of("\""); //find index of second quotation mark
        trimmed2.append(trimmed.substr(0, second));

        return trimmed2; // returns trimmed string with no quotation marks
    }
    return trimmed; // returns trimmed string
}

// specific version of trim for the awk function:
string awkTrim(string line)
{
    if (line.find("\'") == -1)
    {
        return line;
    }
    size_t second = line.find_last_of("\'");
    line = line.substr(1, second - 1);
    while (line.find(" ") != -1)
    {
        size_t space = line.find_last_of(" ");
        line = line.substr(0, space) + line.substr(space + 1);
    }

    return line;
}

vector<string> split(string line, string separator = " ")
{

    vector<string> out;
    size_t start;
    size_t end = 0;
    while ((start = line.find_first_not_of(separator, end)) != string::npos)
    {
        end = line.find(separator, start);
        out.push_back(line.substr(start, end - start));
    }
    return out;
}

char **vec_to_char_array(vector<string> parts)
{
    char **array = new char *[parts.size() + 1];
    for (int i = 0; i < parts.size(); i++)
    {
        array[i] = new char[parts[i].size()];
        strcpy(array[i], parts[i].c_str());
    }
    array[parts.size()] = NULL;
    return array;
}

int main()
{
    dup2(0, 3);
    dup2(1, 4);
    vector<int> bgs;
    vector<int> zombie;
    vector<string> directories;

    while (true)
    {
        for (int i = 0; i < zombie.size(); i++)
        {
            int pid = waitpid(zombie[i], 0, WNOHANG);
            if (pid != 0)
            {
                // do nothing
            }
            else
            {
                zombie.pop_back();
                // pop it fron zombie vector
            }
        }

        dup2(3, 0);
        dup2(4, 1);

        // personalized function
        time_t currTime = time(NULL); // reference from client.cpp (PA1)
        tm *currPointer = localtime(&currTime);
        int month = currPointer->tm_mon + 1;
        int day = currPointer->tm_mday;
        int year = currPointer->tm_year + 1900;
        int hour = currPointer->tm_hour;
        int min = currPointer->tm_min;
        int sec = currPointer->tm_sec;
        cout << month << "/" << day << "/" << year << " " << hour << ":" << min << ":" << sec << endl; 
        
        // beginning of shell
        cout << "Alexia's Shell$";
        char buffer[1000];
        string currentDir = getcwd(buffer, sizeof(buffer)); // gets the current working directory and stores it in variable
        string inputline;
        getline(cin, inputline);

        inputline = trim(inputline);

        if (inputline == string("exit"))
        {
            cout << "BYE!" << endl;
            break;
        }

        vector<string> pparts = split(inputline, "|");

        // do this for each pipe
        for (int i = 0; i < pparts.size(); i++)
        {
            int fds[2];
            pipe(fds);

            // & background processes
            bool bg = false;
            int bgp = pparts[i].find("&");
            if (bgp != string::npos)
            {
                pparts[i] = pparts[i].substr(0, bgp - 1);
                bg = true;
                cerr << "Found a background process" << endl;
            }

            // cd into directory
            if (trim(inputline).find("cd") == 0)
            {
                directories.push_back(currentDir); // stores current directory in vector before cd into new directory
                string dirname = trim(split(inputline, " ")[1]);
                if (dirname == "-")
                {
                    // cd into prev directory
                    dirname = directories.at(directories.size() - 2); // changes dirname to previous directory before the one we just accessed
                }
                chdir(dirname.c_str());                      // change directory to dirname
                currentDir = getcwd(buffer, sizeof(buffer)); // update the current directory to the new one
                continue;
            }

            int pid = fork();
            if (pid == 0)
            {
                pparts[i] = trim(pparts[i]);
                // awk function testing
                if (pparts[i].find("awk") == 0)
                {
                    pparts[i] = "awk " + awkTrim(pparts[i].substr(pparts[i].find("\'")));
                }

                // ls > a.txt and < a.txt
                int pos1 = inputline.find('>'); // gets the index where ">" is found
                int pos2 = inputline.find('<'); // gets the index where "<" is found
                if (pos1 >= 0)
                {
                    // if the ">" is found
                    string command = inputline.substr(0, pos1);
                    string filename = trim(inputline.substr(pos1 + 1)); // substrings all the way to the end from index pos+1

                    pparts[i] = command;
                    int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_RDONLY, S_IWUSR | S_IRUSR);
                    dup2(fd, 1);
                    close(fd);
                }

                if (pos2 >= 0)
                {
                    // if the "<" is found
                    string command = inputline.substr(0, pos2);
                    string filename = trim(inputline.substr(pos2 + 1)); // substrings all the way to the end from index pos+1

                    pparts[i] = command;
                    int fd = open(filename.c_str(), O_RDONLY | O_CREAT, S_IWUSR | S_IRUSR);
                    dup2(fd, 0);
                    close(fd);
                }

                if (i < pparts.size() - 1)
                {
                    dup2(fds[1], 1);
                }
                
                // echo
                vector<string> parts;

                if (inputline.find("echo") == 0)
                {
                    string print = inputline.substr(4); // print =  "szdjnfsjdf"
                    print = trim(print);                // trimmed print string
                    // handle single quotation marks
                    int first = print.find_first_of("\'");
                    int last = print.find_last_of("\'");
                    if (first != -1 && last != -1)
                        print = print.substr(first + 1, last - 1);
                    // end of single quotation handling
                    parts.push_back(inputline.substr(0, 4));
                    parts.push_back(print);
                }
                else
                {
                    parts = split(pparts[i]);
                }
                char **args = vec_to_char_array(parts);
                execvp(args[0], args);
            }

            else
            {
                if (!bg)
                {
                    if (i == pparts.size() - 1)
                    {
                        waitpid(pid, 0, 0);
                    }
                    else
                    {
                        zombie.push_back(pid);
                    }
                }
                else
                {
                    {
                        zombie.push_back(pid);
                    }
                }

                dup2(fds[0], 0);
                close(fds[1]);
            }
        }
    }
}