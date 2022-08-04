/*
compile with:
gcc -Wall -g -ljson-c -lcrypt -lasound -pthread server.c -o server

run with valgrind to check "LEAK SUMMARY"
valgrind --tool=memcheck --leak-check=full -v ./server 9999
*/
#define  _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#include <alsa/asoundlib.h>
#include <pthread.h>
#include <json-c/json.h>

/*
    JSON functions are depreciated. 

    Connection is not secured in any way. This can be used in SECURE closed network.
    
    You don't like that? do it Yoursself :)
*/

void error(const char *msg)
{
    perror(msg);
    exit(1);
}
//Get system load avarage
char* getLoadAVG()
{
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    /*
        open file /proc/loadavg
        contains load average in form of string like:
        0.00 0.00 0.00 1/130 8059
    */
    fp = fopen("/proc/loadavg", "r");
    char *result;

    //Get line from file
    while ((read = getline(&line, &len, fp)) != -1) {
        result=(char *)line;
    }
    fclose(fp);

    return result;
}

//set and retrive system volume level requires asoundlib.h
//void *setVolume(struct json_object *jobj, char result[])
struct factory{                    
    char buff[3];
    struct json_object *obj;

};  
struct factory assembly;
struct factory eassembly;
void * setVolume(void *param)
{
    //get volume change direction from json
    struct factory *assembly = param;
    char *direction=(char *) json_object_get_string(json_object_object_get(assembly->obj,"direction"));
/////////////////
    int ret = 0;
    snd_mixer_t* handle;
    snd_mixer_elem_t* elem;
    snd_mixer_selem_id_t* sid;
    //prepare for operations can be found in alsa documentation
    static const char* mix_name = "Master";
    static const char* card = "default";
    static int mix_index = 0;

    long get_vol;

    snd_mixer_selem_id_alloca(&sid);

    //sets simple-mixer index and name
    snd_mixer_selem_id_set_index(sid, mix_index);
    snd_mixer_selem_id_set_name(sid, mix_name);

    if ((snd_mixer_open(&handle, 0)) < 0)
        return -1;
    if ((snd_mixer_attach(handle, card)) < 0) {
        snd_mixer_close(handle);
        return -2;
    }
    if ((snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
        snd_mixer_close(handle);
        return -3;
    }
    ret = snd_mixer_load(handle);
    if (ret < 0) {
        snd_mixer_close(handle);
        return -4;
    }
    elem = snd_mixer_find_selem(handle, sid);
    if (!elem) {
        snd_mixer_close(handle);
        return -5;
    }

    long minv, maxv;

    //Get volume range
    snd_mixer_selem_get_playback_volume_range (elem, &minv, &maxv);

    snd_mixer_selem_get_playback_volume(elem,SND_MIXER_SCHN_FRONT_LEFT,&get_vol);

    //if volume in range than change it
    if(strcmp(direction,"up")==0 && get_vol<maxv)
    {
        get_vol=get_vol+1;
    }
    else if(strcmp(direction,"down")==0 && get_vol>minv)
    {
        get_vol=get_vol-1;
    }
    snd_mixer_selem_set_playback_volume(elem,SND_MIXER_SCHN_FRONT_LEFT,get_vol);
    snd_mixer_selem_set_playback_volume(elem,SND_MIXER_SCHN_FRONT_RIGHT,get_vol);

    //get volume after change
    snd_mixer_selem_get_playback_volume(elem,SND_MIXER_SCHN_FRONT_LEFT,&get_vol);

    char useradd_cmd[3];

    snprintf (useradd_cmd, 3, "%li", (char*)get_vol);
    //return volume to var from input
   // strcat(result,useradd_cmd);
   
    strcat(assembly->buff,useradd_cmd);

    free(mix_name);
    return 0;
////////////////
}
//Get system users
char* getUser()
{
    FILE * f;

    char * buffer = 0;
    long length;

    /*
        system does not return command result
        write result to file than read it
        Method is different than in getLoadAVG
    */
    system("cut -d: -f1 /etc/passwd > tmp");

    f = fopen("tmp", "r");

    if (f)
    {
        /*
            Instead of reading line by line get file size
            and read whole file
        */
        fseek (f, 0, SEEK_END);
        length = ftell (f);
        fseek (f, 0, SEEK_SET);
        buffer = malloc (length);
        if (buffer)
        {
            fread (buffer, 1, length, f);
        }
    
        fclose (f);
    }
   // free(buffer);
    return buffer;
}

//Add new user
int userAdd(struct json_object *jobj)
{
    //printf("%s\n",crypt("pass","$6$salt"));
    /*
        We need appropriate salt, it could look like $6$salt$
        In /etc/shadow salt is part of string with password starting $*$*$
        Check description for /etc/shadow
    */

    char *username=(char *) json_object_get_string(json_object_object_get(jobj,"username"));
    char *passwd=(char *) json_object_get_string(json_object_object_get(jobj,"passwd"));

    char *salt=(char *) json_object_get_string(json_object_object_get(jobj,"salt"));

    int len_subject=strlen(salt); 

    int len_passwd=strlen(passwd); 

    char *passwd_cryp=crypt(passwd,salt);

    char *cmd="useradd -m";

    size_t cmd_len = strlen(cmd);

    size_t username_len = strlen(username);

    size_t crypt_len = strlen(passwd_cryp);

    size_t sumlen=cmd_len+username_len+crypt_len+11;

    char useradd_cmd[sumlen];

    snprintf (useradd_cmd, sumlen, "sudo %s %s -p %s", cmd,username,passwd_cryp);

    return system(useradd_cmd);
}

//Repleace substring
char* repleace(char *subject, char *find, char *new)
{
    /*
        In C it is easier to think in terms of repleacing memory containing data 
        than directly repleacing chars(sub strings) like in PHP 
    */
    int len_subject=strlen(subject); 
    int len_find=strlen(find); 
    int len_new=strlen(new);
//
    char *pfound = strstr(subject, find);
    //calculate position
    int dposfound =pfound - subject;
//
    char *start=malloc(*pfound);

    char *end= malloc(len_subject);

    char *result = malloc(len_subject + (len_find - len_new) + 1);

    strncpy(start,subject,dposfound);
    strncpy(end,subject+dposfound+len_find,len_subject-dposfound-len_find);

    snprintf (result, strlen(start)+strlen(new)+strlen(end)+1, "%s%s%s", start, new, end);
    free(start);
    free(end);
    return result;
}

//Create apache2 VirtualHost
void createVirtualHost(struct json_object *jobj)
{
    /*
        Get template and repleace keywords to create new configuration
    */
    FILE * fp;
    FILE * fw;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    printf("createVirtualHost: %s \n", json_object_get_string(json_object_object_get(jobj,"VHost")));

    char *template=(char *) json_object_get_string(json_object_object_get(jobj,"template"));

    char *target=(char *) json_object_get_string(json_object_object_get(jobj,"target"));

    char *vhost=(char *) json_object_get_string(json_object_object_get(jobj,"VHost"));

    char *docRoot=(char *) json_object_get_string(json_object_object_get(jobj,"docRoot"));

///
    fp = fopen(template, "r");
    fw = fopen(target, "w");

    while ((read = getline(&line, &len, fp)) != -1) {

        if(strstr(line,"VHost"))
        {
             fputs(repleace(line, "VHost", vhost),fw);
        }
        else if(strstr(line,"docRoot"))
        {
             fputs(repleace(line, "docRoot", docRoot),fw);
        }
        else
            fputs(line,fw);
    }
    fclose(fp);
    fclose(fw);
    system("/etc/init.d/apache2 restart");
///
}
//////////////

int main(int argc, char *argv[])
{

    int sockfd, newsockfd, portno;
//
    struct json_object *jobj;

    socklen_t clilen;
     char buffer[256];
     struct sockaddr_in serv_addr, cli_addr;

     int n;
     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     listen(sockfd,5);

     clilen = sizeof(cli_addr);
////
    while(1==1)
    {
         newsockfd = accept(sockfd, 
                 (struct sockaddr *) &cli_addr, 
                 &clilen);
        if (newsockfd < 0) 
          error("ERROR on accept");
        bzero(buffer,256);
         n = read(newsockfd,buffer,255);
         if (n < 0) error("ERROR");
//
        jobj = json_tokener_parse(buffer);

        pthread_t thread_id;
        assembly.obj =jobj;


        if(json_object_get_type(jobj)==4)
        {
            char *action=(char *) json_object_get_string(json_object_object_get(jobj,"action"));

            if(strcmp(action,"addhost")==0)
            {
                createVirtualHost(jobj);
                //pthread_create(&thread_id, NULL, createVirtualHost(jobj), NULL);
            }
            else if(strcmp(action,"useradd")==0)
            {
                printf("UserAdd %d \n",userAdd(jobj));
            }
            else if(strcmp(action,"getuser")==0)
            {
                char *buff=getUser(jobj);
                
                write(newsockfd,buff,strlen(buff));
                //free buffer = malloc (length); from getUser
                free(buff);
            }
            else if(strcmp(action,"avg")==0)
            {
                char *buff=getLoadAVG(jobj);
                n = write(newsockfd,buff,strlen(buff));

            }
            else if(strcmp(action,"setvolume")==0)
            {
                //printf("SET VOLUME \n");
                pthread_create(&thread_id, NULL, setVolume,  &assembly);
                pthread_join(thread_id, NULL);
                n = write(newsockfd, assembly.buff,strlen(assembly.buff));

                assembly=eassembly;
            }
            else
            {
                printf("ELSE %s \n",action);
            }
            
        }

        free(jobj);

        n = write(newsockfd,"OK",2);
        if (n < 0) error("ERROR");
    }
    close(newsockfd);
    close(sockfd);

    return 0; 
//
}
