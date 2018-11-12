//Edward Hu

#include <getopt.h> //getopt
#include <unistd.h> //
#include <stdio.h> //
#include <stdlib.h>  //
#include <errno.h> //errno
#include <string.h> //strerr
#include <sys/types.h> //wait
#include <sys/wait.h> //wait
#include <poll.h> //poll
#include <signal.h> //kill
#include <sys/socket.h> //socket
#include <netinet/in.h> //socket
#include <string.h> //memmove
#include "zlib.h" //zlib

void error_report (int error, char* eventstr)
{
  fprintf(stderr, "Error occured when %s because %s, exiting now...", eventstr, strerror(error));
  exit(1);
}


int main (int argc, char *argv[])
{
  
  static int port_flag = 0;
  static int compress_flag = 0;
  int optchar;
  int errnum;
  int portnum;

  const int BUFFER_SIZE = 15000;
  char buff1[BUFFER_SIZE];
  char buff2[BUFFER_SIZE];
  char chunk1[BUFFER_SIZE];
  char chunk2[BUFFER_SIZE];
  int r_ret;
  int w_ret;
  int whilebreak = 0;
  char combo[] = {'\r','\n'};
  char lf[] = {'\n'};

  int sockfd;
  int newsockfd;
  unsigned int clilen;
  struct sockaddr_in serv_addr;
  struct sockaddr_in cli_addr;

  static int shutdown_flag = 0;

  while (1) //go through all options
    {
      struct option longopts[] = {
	{ "port", required_argument, NULL, 'p'},
	{ "compress", no_argument, &compress_flag, 1},
	{ 0, 0, 0, 0}
      };
      int optindex = 0;
      optchar = getopt_long(argc, argv, "", longopts, &optindex);

      //if at end of options
      if (optchar == -1)
	break;

      switch (optchar) {
      case 'p': //port option
	portnum = atoi(optarg);
	port_flag = 1;
	break;
      case 0:
	if(longopts[optindex].flag!=0)
	  {
	    // fprintf(stderr, "flag set for %s\n", longopts[optindex].name);
	    break;
	  }
	break;//not supposed to happen
      case '?': //invalid option
	fprintf(stderr, "The valid option(s) are:\n--port= command line parameter\n--compress\n");
	exit(1);
      default: //not supposed to happen
	fprintf(stderr, "Something went wrong getopt_long returning %c, exiting...\n", optchar);
	exit(1);
      }
    }

  if (port_flag == 0)
    {
      fprintf(stderr, "\"--port= command line parameter\" is a mandatory option. Exiting...\n");
      exit(1);
    }

  //initialize piping
  int pipe1fd[2];
  int pipe2fd[2];

  if (pipe(pipe1fd) == -1)
    {
      errnum = errno;
      error_report(errnum, "creating pipe 1");
    }
  if (pipe(pipe2fd) == -1)
    {
      errnum = errno;
      error_report(errnum, "creating pipe 2");
    }

  //begin socketing
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  errnum = errno;
  if (sockfd < 0)
    error_report(errnum, "creating socket on server's end");
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portnum);
  if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
      errnum = errno;
      error_report(errnum, "binding socket to an address");
    }
  if (listen(sockfd, 4) < 0) //4 is the number of pending connections for sockfd (chosen arbitrarily) a queue waiting for sockfd
    {
      errnum = errno;
      error_report(errnum, "listening for connections on the socket");
    }
  clilen = sizeof(cli_addr);
  newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
  errnum = errno;
  if (newsockfd < 0)
    error_report(errnum, "accepting/awaiting client connection to server");

  //connection is now ready. let the comms begin
  
  int rc = fork();
  errnum = errno;
  if (rc < 0) //fork failed
    error_report(errnum, "forking");
  else if (rc == 0)//child process
    {
      //close unused pipe fds
      if(close(pipe1fd[1]) == -1)
	{
	  errnum = errno;
	  error_report(errnum, "closing write end of pipe 1");
	}
      if(close(pipe2fd[0]) == -1)
	{
	  errnum = errno;
	  error_report(errnum, "closing read end of pipe 2");
	}

      //set stdin as pipe from terminal process
      if(dup2(pipe1fd[0], 0) == -1)
	{
	  errnum = errno;
	  error_report(errnum, "duplicating pipe1fd[0] with stdin");
	}
      if(close(pipe1fd[0]) == -1)
	{
	  errnum = errno;
	  error_report(errnum, "closing read end of pipe 1");
	}

      //set stdout and stderr as dups of a pipe to the terminal process
      if(dup2(pipe2fd[1], 1) == -1)
	{
	  errnum = errno;
	  error_report(errnum, "duplicating pipe2fd[1] with stdout");
	}
      if(dup2(pipe2fd[1], 2) == -1)
	{
	  errnum = errno;
	  error_report(errnum, "duplicating pipe2fd[1] with stderr");
	}
      if(close(pipe2fd[1]) == -1)
	{
	  errnum = errno;
	  error_report(errnum, "closing write end of pipe 2");
	}

      //run shell
      char* childargs[2];
      childargs[0] = "/bin/bash";
      childargs[1] = NULL;
      execv(childargs[0], childargs);
    }
  
  else //parent process
    {
      if(close(pipe1fd[0]) == -1)
	{
	  errnum = errno;
	  error_report(errnum, "closing read end of pipe 1");
	}
      if(close(pipe2fd[1]) == -1)
	{
	  errnum = errno;
	  error_report(errnum, "closing write end of pipe 2");
	}

      struct pollfd poll_list[2];
      int p_ret;
      int timeout = 0;
      int nfds = 2;
      poll_list[0].fd = newsockfd;
      poll_list[1].fd = pipe2fd[0]; //pipe-shell
      poll_list[0].events = POLLIN;
      poll_list[1].events = POLLIN;
     
      if (compress_flag == 0)
	{
	  
	  while (1)
	    {
	      p_ret = poll(poll_list, nfds, timeout);
	      if (p_ret == -1)
		{
		  errnum = errno;
		  error_report(errnum, "polling");
		}
	      else if (p_ret > 0) //gotta catch em all (events)
		{
		  for (int i = 0; i < nfds; i++)
		    {
		      if (((poll_list[i].revents & POLLHUP) == POLLHUP) || ((poll_list[i].revents & POLLERR) == POLLERR))
			{
			  shutdown_flag = 1;
			  whilebreak = 1;
			  break;
			}
		    }
		
		  if((!shutdown_flag) && ((poll_list[0].revents & POLLIN) == POLLIN))
		    {
		      r_ret = read(newsockfd, buff1, BUFFER_SIZE);
		      errnum = errno;
		      if (r_ret < 0)
			error_report(errnum, "reading input from socket on server's end");
		      else if (r_ret > 0) { //succeeded
			for (int i = 0; i<r_ret; i++)
			  {
			    if (buff1[i] == '\004') //Ctrl-D
			      {
				if(close(pipe1fd[1]) == -1)
				  {
				    errnum = errno;
				    error_report(errnum, "closing write end of pipe 1");
				  }
				break;
			      }
			    else if (buff1[i] == '\003') //ctrl-c
			      {
				if (kill(rc, SIGINT) == -1)
				  {
				    errnum = errno;
				    error_report(errnum, "sending SIGINT signal with kill to child");
				  }
			      }
			    else if (buff1[i] == '\n' || buff1[i] == '\r')
			      {
				//forward to pipe-shell
				w_ret = write(pipe1fd[1], lf, 1);
				errnum = errno;
				if (w_ret < 0)
				  error_report(errnum, "writing <lf> to pipe-shell");
			      }
			    else //normal char
			      {
				//forward to pipe-shell
				w_ret = write(pipe1fd[1], (buff1+i), 1);
				errnum = errno;
				if (w_ret < 0)
				  error_report(errnum, "writing socket input into pipe-shell");
			      }
			  } //for loop of r_ret                         	
		      }
		      else
			{ //r_ret == 0, EOF from socket
			  if(close(pipe1fd[1]) == -1)
			    {
			      errnum = errno;
			      error_report(errnum, "closing write end of pipe 1 after EOF from socket");
			    }
			}
		    } //checking revents for POLLIN from socket
		    
		  if((poll_list[1].revents & POLLIN) == POLLIN)
		    {
		      r_ret = read(pipe2fd[0], buff2, BUFFER_SIZE);
		      errnum = errno;
		      if (r_ret < 0) //read error
			error_report(errnum, "reading input from pipe-shell");
		      else if (r_ret >= 0) { //succeeded
			for (int i = 0; i< r_ret; i++)
			  {
			    if (buff2[i] == '\n')
			      {
				w_ret = write(newsockfd, combo, 2);
				errnum = errno;
				if (w_ret < 0)
				  error_report(errnum, "writing <cr><lf> to socket from pipe-shell");
			      }
			    else //normal char
			      {
				w_ret = write(newsockfd, (buff2+i), 1);
				errnum = errno;
				if (w_ret < 0)
				  error_report(errnum, "writing pipe-shell input onto socket");
			      }
			  }

			if (r_ret == 0) //EOF from pipe-shell
			  {
			    shutdown_flag = 1;
			    break;
			  }
		      }
		    }//checking revents for POLLIN from pipe-shell
	    
		  if (whilebreak)
		    break;
		} //else if p_ret > 0
	      
	    }//end of while(1)
	}

      else //compress_flag == 1
	{
	  while (1)
	    {
	      p_ret = poll(poll_list, nfds, timeout);
	      if (p_ret == -1)
		{
		  errnum = errno;
		  error_report(errnum, "polling");
		}
	      else if (p_ret > 0) //gotta catch em all (events)
		{
		  for (int i = 0; i < nfds; i++)
		    {
		      if (((poll_list[i].revents & POLLHUP) == POLLHUP) || ((poll_list[i].revents & POLLERR) == POLLERR))
			{
			  shutdown_flag = 1;
			  whilebreak = 1;
			  break;
			}
		    }
		
		  if((!shutdown_flag) && ((poll_list[0].revents & POLLIN) == POLLIN))
		    {
		      r_ret = read(newsockfd, buff1, BUFFER_SIZE);
		      errnum = errno;
		      if (r_ret < 0)
			error_report(errnum, "reading input from socket on server's end");
		      else if (r_ret > 0) { //succeeded

			//decompress
			z_stream strm;
			strm.zalloc = Z_NULL;
			strm.zfree = Z_NULL;
			strm.opaque = Z_NULL;
			strm.avail_in = 0;
			strm.next_in = Z_NULL;
			if (inflateInit(&strm) != Z_OK)
			  {
			    fprintf(stderr, "inflateInit failed to initialize on server's end. Exiting now...\n");
			    exit(1);
			  }

			strm.avail_in = (uInt)r_ret;
			strm.next_in = (Bytef*)buff1;
			strm.avail_out = (uInt)BUFFER_SIZE;
			strm.next_out = (Bytef*)chunk1;

			do
			  {
			    int ret = inflate(&strm, Z_SYNC_FLUSH);
			    switch (ret) {
			    case Z_NEED_DICT:
			    case Z_STREAM_ERROR:
			    case Z_DATA_ERROR:
			    case Z_MEM_ERROR:
			      fprintf(stderr, "inflate resulted in error on server's side. Exiting now...\n");
			      inflateEnd(&strm);
			      exit(1);
			    }
			  } while (strm.avail_in > 0);

			int compressed = BUFFER_SIZE - strm.avail_out;
			inflateEnd(&strm);
			
			for (int i = 0; i<compressed; i++)
			  {
			    if (chunk1[i] == '\004') //Ctrl-D
			      {
				if(close(pipe1fd[1]) == -1)
				  {
				    errnum = errno;
				    error_report(errnum, "closing write end of pipe 1");
				  }
				break;
			      }
			    else if (chunk1[i] == '\003') //ctrl-c
			      {
				if (kill(rc, SIGINT) == -1)
				  {
				    errnum = errno;
				    error_report(errnum, "sending SIGINT signal with kill to child");
				  }
			      }
			    else if (chunk1[i] == '\n' || chunk1[i] == '\r')
			      {
				//forward to pipe-shell
				w_ret = write(pipe1fd[1], lf, 1);
				errnum = errno;
				if (w_ret < 0)
				  error_report(errnum, "writing <lf> to pipe-shell");
			      }
			    else //normal char
			      {
				//forward to pipe-shell
				w_ret = write(pipe1fd[1], (chunk1+i), 1);
				errnum = errno;
				if (w_ret < 0)
				  error_report(errnum, "writing socket input into pipe-shell");
			      }
			  } //for loop of r_ret     
		       
		      }
		      else
			{ //r_ret == 0, EOF from socket
			  if(close(pipe1fd[1]) == -1)
			    {
			      errnum = errno;
			      error_report(errnum, "closing write end of pipe 1 after EOF from socket");
			    }
			}
		    } //checking revents for POLLIN from socket
		    
		  if((poll_list[1].revents & POLLIN) == POLLIN)
		    {
		      r_ret = read(pipe2fd[0], buff2, BUFFER_SIZE);
		      errnum = errno;
		      if (r_ret < 0) //read error
			error_report(errnum, "reading input from pipe-shell");
		      else if (r_ret >= 0) { //succeeded
			
			if (r_ret == 0) //EOF from pipe-shell
			  {
			    shutdown_flag = 1;
			    break;
			  }

			//compress
			z_stream strm;
			strm.zalloc = Z_NULL;
			strm.zfree = Z_NULL;
			strm.opaque = Z_NULL;
			  
			if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK)
			  {
			    fprintf(stderr, "deflateInit failed to initalize on server's end. Exiting now...\n");
			    exit(1);
			  }
			  
			strm.avail_in = (uInt)r_ret;
			strm.next_in = (Bytef*)buff2;
			strm.avail_out = (uInt)BUFFER_SIZE;
			strm.next_out = (Bytef*)chunk2;
			  
			do
			  {
			    if (deflate(&strm, Z_SYNC_FLUSH) == Z_STREAM_ERROR)
			      {
				fprintf(stderr, "deflate failed and resulted in Z_STREAM_ERROR on server side. Exiting now...\n");
				deflateEnd(&strm);
				exit(1);
			      }
			  } while (strm.avail_in > 0);

			int compressed = BUFFER_SIZE - strm.avail_out;
			  
			deflateEnd(&strm);

			w_ret = write(newsockfd, chunk2, compressed);
			errnum = errno;
			if (w_ret < 0)
			  error_report (errnum, "writing compressed pipe-shell input into socket");
		      }
		    }//checking revents for POLLIN from pipe-shell
	    
		  if (whilebreak)
		    break;
		} //else if p_ret > 0
	    
	    }//end of while(1)
	}
      int status = 0;
      if (waitpid(rc, &status ,0) == -1)
	{
	  errnum = errno;
	  error_report(errnum, "wait on child process");
	}

      fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status) , WEXITSTATUS(status));

      //close socket
      if(close(newsockfd) == -1)
	{
	  errnum = errno;
	  error_report(errnum, "closing socket from server's end");
	}

      exit(0);
    }	  //end of parent loop
}
