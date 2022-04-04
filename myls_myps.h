#include <iostream>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h> // getgrgid function
#include <algorithm>
#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include<fstream>

using namespace std;

void myls(){
    vector<string> fileNameList;
    DIR *d;
    struct dirent *dir;
    struct stat fileStat;
    char time[80];

    d = opendir(".");                   //Get all the files in current directory
    if (d) {
        while ((dir = readdir(d)) != NULL) {
                fileNameList.push_back(dir->d_name);       //Add all the file names to vector
        }
        closedir(d);
    }
    else{
        cout<<"Failed to Open the Current Directory. Error!!!"<<endl;
    }

    sort(fileNameList.begin(),fileNameList.end());


    for(size_t i=0;i<fileNameList.size();i++) {

        if (stat(fileNameList[i].c_str(), &fileStat) < 0) {                             //Get all the file stats
            cout << "Error in getting information of file "<< fileNameList[i] << endl;
        }
        else {
            cout<<(S_ISDIR(fileStat.st_mode) ? "d" : "-");                              //Print all the file stats
            cout<<((fileStat.st_mode & S_IRUSR) ? "r" : "-");
            cout<<((fileStat.st_mode & S_IWUSR) ? "w" : "-");
            cout<<((fileStat.st_mode & S_IXUSR) ? "x" : "-");
            cout<<((fileStat.st_mode & S_IRGRP) ? "r" : "-");
            cout<<((fileStat.st_mode & S_IWGRP) ? "w" : "-");
            cout<<((fileStat.st_mode & S_IXGRP) ? "x" : "-");
            cout<<((fileStat.st_mode & S_IROTH) ? "r" : "-");
            cout<<((fileStat.st_mode & S_IWOTH) ? "w" : "-");
            cout<<((fileStat.st_mode & S_IXOTH) ? "x" : "-");
            cout<<"\t";
            cout<<fileStat.st_nlink;
            cout<<"\t";
            cout<<getpwuid(fileStat.st_uid)->pw_name;
            cout<<"\t";
            cout<<getgrgid(fileStat.st_gid)->gr_name;
            cout<<"\t";
            cout<<fileStat.st_size;
            cout<<"\t";
            strftime(time, 80, "%b %d %I:%M", localtime(&fileStat.st_mtime));
            cout<<time;
            cout<<"\t";
            cout<<fileNameList[i];
            cout<<endl;

        }
    }
}

void getUID_PPID_C(char* pid, char*UID, int &PPID, int &CCC){
    FILE *filePointer ;
    char path[256];
    char *information = new char [256];
    sprintf(path, "/proc/%s/status", pid);
    char *inf;
    filePointer = fopen(path, "r") ;

    for(int i = 0; i< 5; i++){ // ignore first 4 lines
        fgets ( information, 255, filePointer );
    }
    
    //extract C information from the process information
    inf = strtok(information,"\t"); // parse with space
    inf = strtok(NULL, "\t"); // parse with space
    CCC = atoi(inf);

    fgets ( information, 255, filePointer );
    fgets ( information, 255, filePointer );

    //extract ppid information
    inf = strtok(information,"\t"); // parse with space
    inf = strtok(NULL, "\t"); // parse with space
    PPID = atoi(inf);
    fgets ( information, 255, filePointer ); //ignore 9th line
    fgets ( information, 255, filePointer );
    
    
    //extract UID information
    inf = strtok(information,"\t"); // parse with space
    inf = strtok(NULL, "\t"); // parse with space
    
    if(atoi(inf) == 0){
        strcpy(UID, "root");
    }
    else {
        getlogin_r(UID,256);
    }
}

void myps(){
    DIR *directory;
    int fd1;
    int fd2;
    unsigned long currTime, startTime;
    char flag, flagReq, *tty;
    char cmd[256];
    char tty_self[256];
    char path[256];
    char UID[256];
    int PPID;
    int CCC;
    int i;
    char time_s[256];
    FILE* file;
    struct dirent *ent;
    
    directory = opendir("/proc");
    fd1 = open("/proc/self/fd/0", O_RDONLY);
    sprintf(tty_self, "%s", ttyname(fd1));
    cout<<"UID \t PID \t PPID \t C \t TTY \t TIME \t\t CMD\n";    
    
    while ((ent = readdir(directory)) != NULL) {
        flag = 1;
        for (i = 0; ent->d_name[i]; i++) {
            if (!isdigit(ent->d_name[i])){ 
                flag = 0;
                break;
            }
        }
        if (flag) {
            sprintf(path, "/proc/%s/fd/0", ent->d_name);
            fd2 = open(path, O_RDONLY);
                
            sprintf(path, "/proc/%s/stat", ent->d_name);
            file = fopen(path, "r");
            fscanf(file, "%d%s%c%c%c", &i, cmd, &flag, &flag, &flagReq);
            cmd[strlen(cmd) - 1] = '\0';
            

            //read currTime from the file
            for (i = 0; i < 11; i++)
                fscanf(file, "%lu", &currTime);
            fscanf(file, "%lu", &startTime);
            currTime = (int)((double)(currTime + startTime) / sysconf(_SC_CLK_TCK));
            sprintf(time_s, "%02lu:%02lu:%02lu",(currTime / 3600) % 3600, (currTime / 60) % 60, currTime % 60);

            tty = ttyname(fd2);
            ent->d_name[5] = '\0';
            time_s[8] = '\0';
            getUID_PPID_C(ent->d_name, UID, PPID, CCC);
            cout<<UID<<" \t ";
            cout<<ent->d_name<<" \t " <<PPID<<" \t " <<CCC<<"\t";
            if(tty)
                for(int i = 5; tty[i]!='\0'; i++){
                    cout<<tty[i];
                }
            else{
                cout<<"?";
            }
            cout<<" \t"<<time_s<<" \t "<<cmd+1 <<endl;
            
            fclose(file);
            close(fd2);
        }
    }
    close(fd1);
}
