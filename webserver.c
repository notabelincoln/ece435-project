#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE	256

/* Default port to listen on */
#define DEFAULT_PORT	8080

int main(int argc, char **argv) {

	int socket_fd, new_socket_fd, file_fd; // file descriptors
	int on;
	int file_len; // length of file contents
	struct sockaddr_in server_addr;
	struct sockaddr client_addr;
	int port=DEFAULT_PORT;
	int n, i;
	short fext;
	socklen_t client_len;

	char buffer_in[BUFFER_SIZE]; // buffer for incoming request
	char buffer_out[BUFFER_SIZE*3]; // buffer for outgoing data
	char buffer_file[BUFFER_SIZE]; // buffer for file contents
	char filepath[64]; // buffer for filepath
	char header[BUFFER_SIZE]; // header string
	char str_date[64]; // string for date and time
	char str_mod[64]; // string for modification time
	char str_ftype[32]; // file type string
	char *fileptr, *fileend;
	
	time_t time_raw;
	struct tm *time_data, *time_mod;
	struct stat file_stat;

	printf("Starting server on port %d\n",port);

	/* Open a socket to listen on */
	/* AF_INET means an IPv4 connection */
	/* SOCK_STREAM means reliable two-way connection (TCP) */
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd<0) {
		fprintf(stderr,"Error opening socket! %s\n",
				strerror(errno));
	}

	/* Set up the server address to listen on */
	/* The memset sets the address to 0.0.0.0 which means */
	/* listen on any interface. */
	memset(&server_addr,0,sizeof(struct sockaddr_in));
	server_addr.sin_family=AF_INET;
	/* Convert the port we want to network byte order */
	server_addr.sin_port=htons(port);

	on=1;
	n = setsockopt(socket_fd,SOL_SOCKET,SO_REUSEADDR,(char *)&on,sizeof(on));
	if (n<0) {
		fprintf(stderr,"Error setsockopt. %s\n",
				strerror(errno));
	}

	/* Bind to the port */
	if (bind(socket_fd, (struct sockaddr *) &server_addr,
				sizeof(server_addr)) <0) {
		fprintf(stderr,"Error binding! %s\n", strerror(errno));
	}

	/* Tell the server we want to listen on the port */
	/* Second argument is backlog, how many pending connections can */
	/* build up */
	while(1) {
		listen(socket_fd,5);

		/* Call accept to create a new file descriptor for an incoming */
		/* connection.  It takes the oldest one off the queue */
		/* We're blocking so it waits here until a connection happens */
		client_len=sizeof(client_addr);
		new_socket_fd = accept(socket_fd,
				(struct sockaddr *)&client_addr,&client_len);
		if (new_socket_fd<0) {
			fprintf(stderr,"Error accepting! %s\n",strerror(errno));
		}
		/* Clear all buffers of any data  */
		memset(buffer_in,0,BUFFER_SIZE);
		memset(buffer_out,0,BUFFER_SIZE*3);
		memset(buffer_file,0,BUFFER_SIZE);
		memset(filepath,0,64);
		memset(header,0,BUFFER_SIZE);
		memset(str_date,0,64);
		memset(str_mod,0,64);
		memset(str_ftype,0,32);

		/* Someone connected!  Let's try to read BUFFER_SIZE-1 bytes */
		n = read(new_socket_fd,buffer_in,(BUFFER_SIZE-1));
		if (n==0) {
			fprintf(stderr,"Connection to client lost\n\n");
		}
		else if (n<0) {
			fprintf(stderr,"Error reading from socket %s\n",
					strerror(errno));
		}


		/* Print the message we received */
		printf("Message from client: %s\n",buffer_in);

		/* Find the filename in the request  */
		fileptr = strstr(buffer_in,"GET")+5;
		fileend = strstr(buffer_in,"HTTP")-1;

		/* Write the filename into the filepath  */
		fext = (fileend - fileptr) + 1;
		for(i=0; fileptr+i != fileend; i++) {
			if ((fext >= 0) && (fext <= (fileend - fileptr)))
				str_ftype[i-fext-1] = fileptr[i];

			filepath[i] = fileptr[i];

			if (fileptr[i] == '.')
				fext = i;
		}
		filepath[fileend-fileptr] = 0;
		fflush(stdout);
		printf("filepath: \"%s\"\n",filepath);
		printf("file extension: \"%s\"\n",str_ftype);

		/* Write correct file type display */
		if (!(strcmp(str_ftype,"html"))) {
			sprintf(str_ftype,"text/html\r\n");
		} else if (!(strcmp(str_ftype,"txt"))) {
			sprintf(str_ftype,"text/plain\r\n");
		} else if (!(strcmp(str_ftype,"png"))) {
			sprintf(str_ftype,"image/png\r\n");
		} else if (!(strcmp(str_ftype,"jpg"))) {
			sprintf(str_ftype,"image/jpeg\r\n");
		} else {
			sprintf(str_ftype,"text/plain\r\n");
		}

		/* Get the time of the current request */
		time(&time_raw);
		time_data = gmtime(&time_raw);
		strftime(str_date, 63, "%a, %d %b %Y %X GMT",time_data);

		/* Try to open and read the file specified in filepath */

		file_fd=open(filepath, O_RDONLY);
		if (file_fd<0) {
			fprintf(stderr,"Error opening %s\n",filepath);

			/* Set the header for a 404 error */
			sprintf(header, "HTTP/1.1 404 Not Found\r\nDate: %s\r\nServer: ECE435\r\nConnection: close\r\nContent-Type: %s\r\n\r\n", str_date,str_ftype);

			/* HTML data for an error 404 */
			sprintf(buffer_file,"<html><head>\r\n<title>404 Not Found</title>\r\n</head><body>\r\n<h1>Not Found<h1>\r\n<p>The requested URL was not found on the server.<br />\r\n</p>\r\n</body></html>\r\n");
		} else {
			/* Get file attributes */
			n=stat(filepath, &file_stat);
			printf("File stat on \"%s\" success\n",filepath);
			if (n<0) fprintf(stderr,"Error stat. %s\n",strerror(errno));
			file_len=file_stat.st_size;
			time_mod=gmtime(&(file_stat.st_mtim.tv_sec));
			strftime(str_mod,63,"%a, %d %b %Y %X GMT",time_mod);

			/* Store the appropriate header for a success */
			sprintf(header, "HTTP/1.1 200 OK\r\nDate: %s\r\nServer: ECE435\r\nLast-Modified: %s\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n",str_date,str_mod,file_len,str_ftype);
			/* Read the file data into its own buffer */
			n = 1;
			while (n) n=read(file_fd,buffer_file,BUFFER_SIZE-1);
		}
		sprintf(buffer_out,"%s%s",header,buffer_file);
		n=write(new_socket_fd,buffer_out,BUFFER_SIZE*3);
	}


	/* Send a response */
	printf("Exiting server\n\n");

	/* Close the sockets */
	close(new_socket_fd);
	close(socket_fd);

	return 0;
}

