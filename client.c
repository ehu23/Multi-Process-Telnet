//Edward Hu

#include <getopt.h> //getopt
#include <unistd.h> //term
#include <stdio.h> //term
#include <stdlib.h>  //term
#include <termios.h> //term
#include <errno.h> //errno
#include <string.h> //strerr
#include <sys/types.h> //wait
#include <sys/wait.h> //wait
#include <poll.h> //poll
#include <signal.h> //kill
#include <sys/socket.h> //socket
#include <netinet/in.h> //socket
#include <netdb.h> //client stuff (hostent)
#include <string.h> //memmove
#include <sys/stat.h> //creat
#include <fcntl.h> //creat
#include "zlib.h" //zlib

//variable that remembers original terminal attributes
struct termios saved_attributes; 

void reset_input_mode (void)
{
  tcsetattr (STDIN_FILENO, TCSANOW, &saved_attributes);
}

void set_input_mode (void)
{
  struct termios tattr;

  /* Make sure stdin is a terminal. */
  if (!isatty (STDIN_FILENO))
    {
      fprintf (stderr, "Not a terminal.\n");
      exit(1);
    }

  /* Save the terminal attributes so we can restore them later. */
  tcgetattr (STDIN_FILENO, &saved_attributes);
  atexit (reset_input_mode);

  /* Set the funny terminal modes. */
  tcgetattr (STDIN_FILENO, &tattr);
  //tattr.c_lflag &= ~(ICANON|ECHO); /* Clear ICANON and ECHO. */
  //tattr.c_cc[VMIN] = 1;
  //tattr.c_cc[VTIME] = 0;
  tattr.c_iflag = ISTRIP;	/* only lower 7 bits	*/
  tattr.c_oflag = 0;		/* no processing	*/
  tattr.c_lflag = 0;		/* no processing	*/
  tcsetattr (STDIN_FILENO, TCSANOW, &tattr);
}

void error_report (int error, char* eventstr)
{
  fprintf(stderr, "Error occured when %s because %s, exiting now...", eventstr, strerror(error));
  exit(1);
}


int main (int argc, char *argv[])
{
  
  static int port_flag = 0;
  static int log_flag = 0;
  static int compress_flag = 0;
  int optchar;
  int errnum;
  int portnum;
  char* logarg;
  int logfd;
  
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
  struct sockaddr_in serv_addr;
  struct hostent *server;
  
  
  static int shutdown_flag = 0;
  
  while (1) //go through all options
    {
      struct option longopts[] = {
	{ "port", required_argument, NULL, 'p'},
	{ "log", required_argument, NULL, 'l'},
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
      case 'l': //log option
	logarg = optarg;
	log_flag = 1;
	break;	 
      case 0:
	if(longopts[optindex].flag!=0){
	  // fprintf(stderr, "flag set for %s\n", longopts[optindex].name);
	  break;
	}
	break; //not supposed to happen
      case '?': //invalid option
	fprintf(stderr, "The valid option(s) are:\n--port= command line parameter\n--log=filename\n--compress\n");
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

  if (log_flag)
    {
      logfd = creat(logarg, 0666);
      errnum = errno;
      if (logfd < 0)
	error_report(errnum, "creating log file");
    }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  errnum = errno;
  if (sockfd < 0)
      error_report(errnum, "creating socket on client's end");
  server = gethostbyname("localhost");
  if (server == NULL)
    {
      fprintf(stderr, "Something went wrong with gethostbyname(\"localhost\"), exiting...\n");
      exit(1);
    }
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  memmove((char*)&serv_addr.sin_addr.s_addr, (char*)server->h_addr, server->h_length);
  serv_addr.sin_port = htons(portnum);
  if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
    {
      errnum = errno;
      error_report(errnum, "connecting to server with connect(2)");
    }
  
  //connection established, start up the comms

  set_input_mode ();

  struct pollfd poll_list[2];
  int keyboardfd = 0; //STDIN
  int p_ret;
  int timeout = 0;
  int nfds = 2;
  poll_list[0].fd = keyboardfd; 
  poll_list[1].fd = sockfd; //socket
  poll_list[0].events = POLLIN;
  poll_list[1].events = POLLIN | POLLOUT;
  
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
	  else if (p_ret > 0) //we caught an event!
	    {
	      for (int i = 0; i < nfds; i++)
		{
		  if (((poll_list[i].revents & POLLHUP) == POLLHUP) || ((poll_list[i].revents & POLLERR) == POLLERR))
		    { //shutdown
		      shutdown_flag = 1;
		      whilebreak = 1;
		      break;
		    }
		}
	      //reading from socket
	      if((poll_list[1].revents & POLLIN) == POLLIN)
		{
		  r_ret = read(sockfd, buff2, BUFFER_SIZE);
		  errnum = errno;
		  if (r_ret < 0)
		    error_report(errnum, "reading input from socket on client's end");
		  else if (r_ret >= 0)
		    { //succeeded
		      for (int i = 0; i< r_ret; i++)
			{
			  w_ret = write(1, (buff2+i), 1);
			  errnum = errno;
			  if (w_ret < 0)
			    error_report(errnum, "writing socket input onto screen");
			}
		      if (r_ret == 0) //EOF from socket
			{
			  shutdown_flag = 1;
			  break;
			}
		 
		      if (log_flag) //r_ret > 0 since it didn't break from previous line
			{
			  w_ret = write(logfd, "RECEIVED ", 9);
			  errnum = errno;
			  if (w_ret < 0)
			    error_report(errnum, "writing \"RECEIVED \" into logfile");

			  char intstr[12]; //all representable ints will fit in 12 char array
			  int numchars = sprintf(intstr, "%d", r_ret);
			  w_ret = write(logfd, intstr, numchars);
			  errnum = errno;
			  if (w_ret < 0)
			    error_report(errnum, "writing the literal number of bytes reading from socket into logfile");

			  w_ret = write(logfd, " bytes: ", 8);
			  errnum = errno;
			  if (w_ret < 0)
			    error_report(errnum, "writing \" bytes: \" reading from socket into logfile");

			  w_ret = write(logfd, buff2, r_ret);
			  errnum = errno;
			  if (w_ret < 0)
			    error_report(errnum, "writing socket read into logfile");

			  w_ret = write(logfd, lf, 1);
			  errnum = errno;
			  if (w_ret < 0)
			    error_report(errnum, "writing \"\n\" for socket read into logfile");
			}

		    }
		}

	  
	      if((!shutdown_flag) && ((poll_list[0].revents & POLLIN) == POLLIN))
		{
		  r_ret = read(keyboardfd, buff1, BUFFER_SIZE);
		  errnum = errno;
		  if (r_ret < 0)
		    error_report(errnum, "reading input from keyboard");

		  //else if (r_ret >=0) 
		  if ((poll_list[1].revents & POLLOUT) == POLLOUT)
		    {
		      if (r_ret > 0)
			{
			  for (int i = 0; i<r_ret; i++)
			    {
			      if (buff1[i] == '\n' || buff1[i] == '\r')
				{//echo to display
				  w_ret = write(1, combo, 2);
				  errnum = errno;
				  if (w_ret < 0)
				    error_report(errnum, "echoing <cr><lf> to display from client's end");
				}
		     
			      else //echo to display
				{
				  w_ret = write(1, (buff1+i), 1);  //special syntax: buff1+i
				  errnum = errno;
				  if (w_ret < 0)
				    error_report(errnum, "writing keyboard input onto screen");
				}
		      
			      //forward to server through socket
			      w_ret = write(sockfd, (buff1+i), 1);
			      errnum = errno;
			      if (w_ret < 0)
				error_report(errnum, "writing keyboard input into socket");
			    } //for loop of r_ret
		      
			  if (log_flag) //kind of cheated cuz I'm logging the read bytes from keyboard, not the write bytes to socket, just assumed they are the same
			    {
			      w_ret = write(logfd, "SENT ", 5);
			      errnum = errno;
			      if (w_ret < 0)
				error_report(errnum, "writing \"SENT \" into logfile");

			      char intstr[12]; //all representable ints will fit in 12 char array
			      int numchars = sprintf(intstr, "%d", r_ret);
			      w_ret = write(logfd, intstr, numchars);
			      errnum = errno;
			      if (w_ret < 0)
				error_report(errnum, "writing the literal number of bytes written to socket into logfile");

			      w_ret = write(logfd, " bytes: ", 8);
			      errnum = errno;
			      if (w_ret < 0)
				error_report(errnum, "writing \" bytes: \" written to socket into logfile");

			      w_ret = write(logfd, buff1, r_ret);
			      errnum = errno;
			      if (w_ret < 0)
				error_report(errnum, "writing socket write into logfile");

			      w_ret = write(logfd, lf, 1);
			      errnum = errno;
			      if (w_ret < 0)
				error_report(errnum, "writing \"\\n\" for socket write into logfile");
			    }
			}
		    }
		  else
		    fprintf(stderr, "POLLIN was set for keyboard but POLLOUT was not set for socket and shutdown was not detected, data lost for most recent keyboard input. Continuing...");
		} //checking revents for POLLIN for keyboard
	  
	      if (whilebreak)
		break;
	    } //else if p_ret > 0
	    
	}//end of while(1)
    }//if compress_flag == 0

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
	  else if (p_ret > 0) //we caught an event!
	    {
	      for (int i = 0; i < nfds; i++)
		{
		  if (((poll_list[i].revents & POLLHUP) == POLLHUP) || ((poll_list[i].revents & POLLERR) == POLLERR))
		    { //shutdown
		      shutdown_flag = 1;
		      whilebreak = 1;
		      break;
		    }
		}
	      //reading from socket
	      if((poll_list[1].revents & POLLIN) == POLLIN)
		{
		  r_ret = read(sockfd, buff2, BUFFER_SIZE);
		  errnum = errno;
		  if (r_ret < 0)
		    error_report(errnum, "reading input from socket on client's end");
		  else if (r_ret >= 0)
		    { //succeeded
		      
		      if (r_ret == 0) //EOF from socket
			{
			  shutdown_flag = 1;
			  break;
			}

		      if (log_flag) //r_ret > 0 since it didn't break from previous line
			{
			  w_ret = write(logfd, "RECEIVED ", 9);
			  errnum = errno;
			  if (w_ret < 0)
			    error_report(errnum, "writing \"RECEIVED \" into logfile");

			  char intstr[12]; //all representable ints will fit in 12 char array
			  int numchars = sprintf(intstr, "%d", r_ret);
			  w_ret = write(logfd, intstr, numchars);
			  errnum = errno;
			  if (w_ret < 0)
			    error_report(errnum, "writing the literal number of bytes reading from socket into logfile");

			  w_ret = write(logfd, " bytes: ", 8);
			  errnum = errno;
			  if (w_ret < 0)
			    error_report(errnum, "writing \" bytes: \" reading from socket into logfile");

			  w_ret = write(logfd, buff2, r_ret);
			  errnum = errno;
			  if (w_ret < 0)
			    error_report(errnum, "writing socket read into logfile");

			  w_ret = write(logfd, lf, 1);
			  errnum = errno;
			  if (w_ret < 0)
			    error_report(errnum, "writing \"\n\" for socket read into logfile");
			}

		      //decompress
		      z_stream strm;
		      strm.zalloc = Z_NULL;
		      strm.zfree = Z_NULL;
		      strm.opaque = Z_NULL;
		      strm.avail_in = 0;
		      strm.next_in = Z_NULL;
		      if (inflateInit(&strm) != Z_OK)
			{
			  fprintf(stderr, "inflateInit failed to initialize on client's end. Exiting now...\n");
			  exit(1);
			}

		      strm.avail_in =(uInt)r_ret;
		      strm.next_in = (Bytef *)buff2;
		      strm.avail_out = (uInt)BUFFER_SIZE;
		      strm.next_out = (Bytef *)chunk2;

		      do
			{
			  int ret = inflate(&strm, Z_SYNC_FLUSH);
			  switch (ret) {
			  case Z_NEED_DICT:
			  case Z_STREAM_ERROR:
			  case Z_DATA_ERROR:
			  case Z_MEM_ERROR:
			    fprintf(stderr, "inflate resulted in error on client's side. Exiting now...\n");
			    inflateEnd(&strm);
			    exit(1);
			  }
			} while (strm.avail_in > 0);

		      int compressed = BUFFER_SIZE - strm.avail_out;
		      inflateEnd(&strm);

		      for (int i = 0; i<compressed; i++)
			{
			  if (chunk2[i] == '\n')
			    {
			      w_ret = write(1, combo, 2);
			      errnum = errno;
			      if (w_ret < 0)
				error_report(errnum, "writing <cr><lf> to display from compressed socket");
			    }
			  else //normal char
			    {
			      w_ret = write(1, (chunk2+i), 1);
			      errnum = errno;
			      if (w_ret < 0)
				error_report(errnum, "writing compressed socket to display");
			    }
			}

		    }
		}

	  
	      if((!shutdown_flag) && ((poll_list[0].revents & POLLIN) == POLLIN))
		{
		  r_ret = read(keyboardfd, buff1, BUFFER_SIZE);
		  errnum = errno;
		  if (r_ret < 0)
		    error_report(errnum, "reading input from keyboard");

		  //else if (r_ret >=0) 
		  if ((poll_list[1].revents & POLLOUT) == POLLOUT)
		    {
		      if (r_ret > 0)
			{
			  for (int i = 0; i<r_ret; i++)
			    {
			      if (buff1[i] == '\n' || buff1[i] == '\r')
				{//echo to display
				  w_ret = write(1, combo, 2);
				  errnum = errno;
				  if (w_ret < 0)
				    error_report(errnum, "echoing <cr><lf> to display from client's end");
				}
		     
			      else //echo to display
				{
				  w_ret = write(1, (buff1+i), 1);  //special syntax: buff1+i
				  errnum = errno;
				  if (w_ret < 0)
				    error_report(errnum, "writing keyboard input onto screen");
				}
			    } //for loop of r_ret

			  //forward compressed
			  z_stream strm;
			  strm.zalloc = Z_NULL;
			  strm.zfree = Z_NULL;
			  strm.opaque = Z_NULL;
			  
			  if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK)
			    {
			      fprintf(stderr, "deflateInit failed to initalize. Exiting now...\n");
			      exit(1);
			    }
			  
			  strm.avail_in = (uInt)r_ret;
			  strm.next_in = (Bytef*)buff1;
			  strm.avail_out = (uInt)BUFFER_SIZE;
			  strm.next_out = (Bytef*)chunk1;
			  
			  do
			    {
			      if (deflate(&strm, Z_SYNC_FLUSH) == Z_STREAM_ERROR)
				{
				  fprintf(stderr, "deflate failed and resulted in Z_STREAM_ERROR. Exiting now...\n");
				  deflateEnd(&strm);
				  exit(1);
				}
			    } while (strm.avail_in > 0);

			  int compressed = BUFFER_SIZE - strm.avail_out;
			  
			  deflateEnd(&strm);

			  w_ret = write(sockfd, chunk1, compressed);
			  errnum = errno;
			  if (w_ret < 0)
			    error_report (errnum, "writing compressed keyboard input into socket");
			  
			  if (log_flag) //kind of cheated cuz I'm logging the read bytes from keyboard, not the write bytes to socket, just assumed they are the same
			    {
			      w_ret = write(logfd, "SENT ", 5);
			      errnum = errno;
			      if (w_ret < 0)
				error_report(errnum, "writing \"SENT \" into logfile");

			      char intstr[12]; //all representable ints will fit in 12 char array
			      int numchars = sprintf(intstr, "%d", compressed);
			      w_ret = write(logfd, intstr, numchars);
			      errnum = errno;
			      if (w_ret < 0)
				error_report(errnum, "writing the literal number of bytes written to socket into logfile");

			      w_ret = write(logfd, " bytes: ", 8);
			      errnum = errno;
			      if (w_ret < 0)
				error_report(errnum, "writing \" bytes: \" written to socket into logfile");

			      w_ret = write(logfd, chunk1, compressed);
			      errnum = errno;
			      if (w_ret < 0)
				error_report(errnum, "writing socket write into logfile");

			      w_ret = write(logfd, lf, 1);
			      errnum = errno;
			      if (w_ret < 0)
				error_report(errnum, "writing \"\\n\" for socket write into logfile");
			    }
			}
		    }
		  else
		    fprintf(stderr, "POLLIN was set for keyboard but POLLOUT was not set for socket and shutdown was not detected, data lost for most recent keyboard input. Continuing...");
		} //checking revents for POLLIN for keyboard
	  
	      if (whilebreak)
		break;
	    } //else if p_ret > 0
	    
	}//end of while(1)

    } //else compress_flag == 1

  exit(0);
}//end of main loop
