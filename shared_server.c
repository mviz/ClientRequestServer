/* Matthew Fritze
 * 1360555
 * CMPUT 379
 * Assignment 2 */

#include "shared_server.h"
#define DATELEN 32
#define ENTRYLEN 256
#define MINLEN 16
#define RESPONSELEN 26
#define OK 200
#define BAD 400
#define FORBIDDEN 403
#define NOTFOUND 404
#define SERVERERR 500

void daemonize(){
    //TODO remove all fprintf messages
	pid_t pid;
	struct rlimit rl;
	struct sigaction sa;

    umask(0);
    
    /* Get maximum number of file descriptors */
    if(getrlimit(RLIMIT_NOFILE, &rl) < 0){
    	fprintf(stderr, "Can't get file limit\n");
        exit(-1);
    }

    /* Become a session leader to lose controlling terminal */
    if((pid = fork()) < 0){
    	fprintf(stderr, "Error forking\n");
        exit(-1);
    }else if (pid > 0){
    	exit(0);
    }
    setsid();

    /* Ensure future opens won't allocate controlling terminals */
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGHUP, &sa, NULL) < 0){
    	fprintf(stderr, "Can't ignore SIGHUP\n");
        exit(-1);
    }
    if((pid = fork()) < 0){
    	  fprintf(stderr, "Can't fork\n");
          exit(-1);
    }
    if(pid > 0){
    	exit(0);
    }

	fclose(stdout);
    fclose(stderr);
    fclose(stdin);
}

int checkArgs(char ** argv){
	int port;
    DIR * serve_dir;

	port = atoi(argv[1]);
    if((port > MAXPORT) || (port < 0)){
        fprintf(stderr, "Port: %d, does not exist\n", port);
        exit(-1);
    }
    // TODO
    serve_dir = opendir(argv[2]); //argv2 is server files
    if(serve_dir == NULL){
        fprintf(stderr, "Directory:%s DNE\n", argv[2] );
        exit(-1);
    }

    //TODO try permission to write to this file
    // log_dir = opendir(argv[3]);
    // if(log_dir == NULL){
    //     fprintf(stderr, "Directory:%s DNE\n", argv[3] );
    //     exit(-1);
    // }

    return port;
}

void logEvent(FILE * logFile, char * request, char * response, int written, int total){
    //TODO make this work lol
    char * date, entry[ENTRYLEN] ,* indx, * head1, * head2;
    date = malloc(DATELEN * sizeof(char));
    getDate(date);
    indx = strstr(response, "OK");

    head1 = getHeader(request);
    head2 = getHeader(response);

    if(!indx) {
        snprintf(entry, ENTRYLEN, "%s\t127.0.0.1\t%s\t%s\n", 
            date, head1, head2);
    } else{
        snprintf(entry, ENTRYLEN, "%s\t127.0.0.1\t%s\t%s\t%d/%d\n", 
            date, head1, head2, written, total);        
    }

    fwrite(entry,1, strlen(entry), logFile);
    free(head1);
    free(head2);
    free(date);
}

void getDate(char * date){
    struct tm * localT;
    time_t rawT;

    time(&rawT);
    localT = localtime(& rawT);

    strftime(date, DATELEN, "%a %d %b %Y %T %Z", localT);
    //return date;
}

char * handleRequest(char * request, char * serverPath){
    char response[RESPONSELEN], * htmlresponse, * rMessage, * date, * fPath;
    int responseSize, fileSize, valid; 
    valid = isValid(request, serverPath); 

    switch(valid){
        case(OK):
            strcpy(response, "200 OK" );
            fPath = malloc(sizeof(char));
            getFileAddr(fPath, serverPath, request);
            htmlresponse = getResponse(fPath);
            break;
        case(BAD):
            strcpy(response, "400 Bad Request");
            htmlresponse = getResponse("400.html");
            break;
        case(FORBIDDEN):
            strcpy(response, "403 Forbidden");
            htmlresponse = getResponse("403.html");
            break;
        case(NOTFOUND):
            strcpy(response, "404 Not Found");
            htmlresponse = getResponse("404.html");
            break;
        case(SERVERERR):
            strcpy(response, "500 Internal Server Error");
            htmlresponse = getResponse("500.html");
            break;
    }

    date = malloc(sizeof(char)*DATELEN);
    getDate(date);

    fileSize = strlen(htmlresponse) - 1;
    responseSize = fileSize + DATELEN + RESPONSELEN + 64; //64 is for the labels
    rMessage = malloc(responseSize * sizeof(char));

    snprintf(rMessage, responseSize, 
        "HTTP/1.1 %s\n%s\nContent-Type: text/html\nContent-Length: %d\r\n\r\n%s",
        response, date, fileSize, htmlresponse);

    free(htmlresponse);
    free(date);
    //free(fPath);
    return rMessage;
}

void getFileAddr(char * fPath, char * sPath, char * request){
    int start = 0, end , reqLen, pathLen, dif, i;
    char c;

    pathLen = strlen(sPath);
    reqLen = strlen(request);

    for(i = 0; i < reqLen; i++){
        c = request[i];
        if(c == ' '){
            if(start == 0){
                start = i + 2;
            }
            else{
                end = i;
                break;
            }
        }
    }
    //TODO make sure the leading char is a /
    if((sPath[pathLen - 1] == '.')){
        sPath[pathLen - 1] = '\0';
        pathLen -= 1;
    }

    if((request[start] == '/') && (pathLen == 0)){
        start++;
    }

    dif = end - start;
    fPath = realloc(fPath, (pathLen + dif + 1) * sizeof(char)); 
    memcpy(fPath , sPath, pathLen);
    memcpy(fPath + pathLen, request  + start, dif); //requested file
}

int isValid(char * request, char * serverPath){
    char  * get = "GET",* http = "HTTP/1.1", c,* getBuff, 
            httpBuff[8], * serveAddr;
    int i, reqLen, start, end;
    FILE * testOpen;

    serveAddr = malloc(sizeof(char));

    reqLen = strlen(request);
    if(reqLen < MINLEN){
        return BAD;
    }
    for(i = 0; i < reqLen; i++){
        c = request[i];
        if(c == ' '){
            if(start == 0){
                start = i;
            }
            else{
                end = i;
                break;
            }
        }
    }

    getBuff = malloc(start - 1);
    memcpy(getBuff, request, start);

    if(strcmp(get, getBuff)){
        free(getBuff);
        return BAD; 
    }

    free(getBuff);


    memcpy(httpBuff, request + (end + 1), 8);
    if(strcmp(http, httpBuff) != 0){
        return BAD;
    }

    /* Test trailing new line */
    if(((request[reqLen - 1] != '\n') || (request[reqLen - 2] != '\n')) &&
       ((request[reqLen - 1] != '\n') || (request[reqLen - 1] != '\r') ||
        (request[reqLen - 3] != '\n'))){
        return BAD;
    }

    getFileAddr(serveAddr, serverPath, request);

    if((serveAddr == NULL)){
        return BAD;
    }
    if((testOpen = fopen(serveAddr, "r"))){ 
        fclose(testOpen);
    }
    else if(errno == ENOENT){
        return NOTFOUND; /* 404 */
    }else if(errno == EACCES){
        return FORBIDDEN; /* 403 */
    }else {
        return SERVERERR; /* 500 */
    }

    free(serveAddr); 

    return OK;
}

char * getResponse(char * addr){
    FILE * fp;
    char * response;
    int fileSize, r, read = 0;

    fp = fopen(addr, "r");

    if(fp == NULL){
        err(1, "Errer opening response file");
    }

    fseek(fp, 0L, SEEK_END);
    fileSize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    response = malloc(fileSize * sizeof(char));

    while(read < fileSize){
        r = fread(response, 1, fileSize, fp);
        if(r == 0){
            err(1, "Read failed");
        }
        read += r;
    }

    fclose(fp);
    return response;
}

char * getHeader(char * buffer){
    int i, c;
    char * header;
    for(i = 0; i < strlen(buffer); i++){
        if((buffer[i] == '\n') || (buffer[i] == '\r')){
            c = i;
            break;
        }
    };

    // while((buffer[c] != '\r') && (buffer[c] != '\n')){
    //     c--;
    // }


    header = malloc(c); // TODO make sure to free
    memcpy(header, buffer, c);

    if(header[strlen(header) - 1] == '\r'){
        header[strlen(header) - 1] = '\0';
    }
    return header;
}