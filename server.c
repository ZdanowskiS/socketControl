#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#include <json-c/json.h>

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

char* getLoadAVG()
{
	FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

	fp = fopen("/proc/loadavg", "r");
	char *result;

	while ((read = getline(&line, &len, fp)) != -1) {
		result=(char *)line;
	}
	fclose(fp);

	return result;
}

char* repleace(char *subject, char *find, char *new)
{

	int len_subject=strlen(subject); 
	int len_find=strlen(find); 
	int len_new=strlen(new);
//
	char *pfound = strstr(subject, find);
	//calculate position
	int dposfound =pfound - subject;
//
	char *start= malloc(pfound);

	char *end= malloc(len_subject);

	char *result = malloc(len_subject + (len_find - len_new) + 1);

	strncpy(start,subject,dposfound);
	strncpy(end,subject+dposfound+len_find,len_subject-dposfound-len_find);

	snprintf (result, strlen(start)+strlen(new)+strlen(end)+1, "%s%s%s", start, new, end);

	return result; 
}

void createVirtualHost(struct json_object *jobj)
{
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

int main(int argc, char *argv[])
{
//
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

		if(json_object_get_type(jobj)==4)
		{
			char *action=(char *) json_object_get_string(json_object_object_get(jobj,"action"));
			if(strcmp(action,"addhost")==0)
			{
				createVirtualHost(jobj);
			}
			else if(strcmp(action,"avg")==0)
			{

				n = write(newsockfd,getLoadAVG(jobj),256);
			}
			else
			{
				printf("ELSE %s \n",action);
			}
		}
//
     	n = write(newsockfd,"OK",2);
     	if (n < 0) error("ERROR");
	}
	close(newsockfd);
	close(sockfd);

	return 0; 
//
}
