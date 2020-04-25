#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <mraa.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <poll.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <openssl/ssl.h>

#define CR 0x0D
#define LF 0x0A
#define EOT 0x04
#define ETX 0x03
#define B 4275
#define R0 100000.0

sig_atomic_t volatile run_flag = 1;

char* id;
char* host;
int portno;
int period = 1;
int f_flag = 1;
int l_flag = 0;
int r_flag = 1;
int error_flag;
FILE *logfile = NULL;
int sockfd;
SSL_CTX *ctx = NULL;
SSL *ssl = NULL;

float convert(int rawtemp, int f_flag)
{
  float R = 1023.0/(float)rawtemp-1.0;
  R *= R0;
  float temp = 1.0/(log(R/R0)/B+1/298.15)-273.15;
  if(f_flag)
    {
      temp = temp * 1.8 + 32;
    }
  return temp;
}

void write_to_server(char* msg)
{
  if(SSL_write(ssl, msg, strlen(msg))< 0)
    {
      fprintf(stderr, "Error writing to server. %s.\n", strerror(errno));
      exit(2);
    }

}

void do_when_interrupted()
{
  run_flag = 0;
}

void process(char* buf)
{
  if(!strcmp(buf, "SCALE=F\n"))
    {
      f_flag = 1;
      if(l_flag)
	fprintf(logfile, "%s", buf);
    }
  else if(!strcmp(buf, "SCALE=C\n"))
    {
      f_flag = 0;
      if(l_flag)
        fprintf(logfile, "%s", buf);
    }
  else if(!strcmp(buf, "STOP\n"))
    {
      r_flag = 0;
      if(l_flag)
        fprintf(logfile, "%s", buf);
    }
  else if(!strcmp(buf, "START\n"))
    {
      r_flag = 1;
      if(l_flag)
        fprintf(logfile, "%s", buf);
    }
  else if(!strcmp(buf, "OFF\n"))
    {
      run_flag = 0;
      if(l_flag)
        fprintf(logfile, "%s", buf);
    }
  else if(!strncmp(buf, "PERIOD=", 7))
    {
      int i;
      int t = strlen(buf);
      char *temp = buf;
      temp[t-1] = '\0';
      for(i = 7; temp[i] != '\0'; i++)
	{
	  if(!isdigit(temp[i]))
	    return;
	}
      period = atoi(temp + 7);
      if(l_flag)
        fprintf(logfile, "%s\n", buf);
    }
  else if(!strncmp(buf, "LOG ", 4))
    {
      if(l_flag)
	{
	  fprintf(logfile, "%s", buf);
	  fprintf(logfile, "%s", buf + 4);
	}
    }
  else
    {
      if(l_flag)
        fprintf(logfile, "%s", buf);
    }
}

int main(int argc, char** argv)
{
  // parse options
  static struct option long_options[] =
  {
    {"id", required_argument, 0, 'i'},
    {"period", required_argument, 0, 'p'},
    {"scale", required_argument, 0, 's'},
    {"host", required_argument, 0, 'h'},
    {"log", required_argument, 0, 'l'},
    {0, 0, 0, 0}
  };

  int c;
  long i;

  while(1)
    {
      c = getopt_long(argc, argv, "", long_options, NULL);
  
      if(c == -1){
	break;
      }
      switch(c)
	{
        case 'p':
          period = atoi(optarg);
          if(period < 0)
            {
              fprintf(stderr, "Wrong argument for --period! The argument should be positive. \n");
              exit(1);
            }
          break;
        case 's':
          if(!strcmp(optarg, "C"))
            {
              f_flag = 0;
            }
	  else if(strcmp(optarg, "F"))
            {
              fprintf(stderr, "Wrong argument for --scale! Only 'C' and 'F' are allowed. \n");
              exit(1);
            }
          break;
	case 'i':
	  id = optarg;
	  for(i = 0; id[i] != '\0'; i++)
	    {
	      if(!isdigit(id[i]))
		{
		  error_flag = 1;
		  break;
		}
	    }
	    if(error_flag || i != 9)
	    {
	      fprintf(stderr, "Wrong argument for --id! id should be a 9-digit-number. \n");
	      exit(1);
	    }
	  break;
	case 'h':
	  host = optarg;
	  break;
	case 'l':
	  l_flag = 1;
	  logfile = fopen(optarg, "w");
	  if(logfile == NULL)
	    {
	      fprintf(stderr, "Cannot create log file. %s.\n", strerror(errno));
	      exit(1);
	    }
	  break;
	default:
	  fprintf(stderr, "Wrong option used! Only --id, --log, --host are allowed.\n");
	  exit(1);
	}
    }

  if(argv[optind] == NULL)
    {
      fprintf(stderr, "Port number is mandatory.\n");
      exit(1);
    }
  else
    {
      portno = atoi(argv[optind]);
    }

  // initialize server connection
  struct sockaddr_in serv_addr;
  struct hostent *server;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0)
    {
      fprintf(stderr, "No port provided. %s.\n", strerror(errno));
      exit(1);
    }

  server = gethostbyname(host);
  if(server == NULL)
    {
      fprintf(stderr, "Error finding host. %s. \n", strerror(errno));
      exit(1);
    }

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,server->h_length);
  serv_addr.sin_port = htons(portno);

  if(connect(sockfd,(struct sockaddr *) &serv_addr, sizeof(serv_addr)) <0)
    {
      fprintf(stderr, "Error connecting. %s.\n", strerror(errno));
      exit(1);
    }

  // initialize SSL
  if(SSL_library_init() < 0)
    {
      fprintf(stderr, "Error initializing SSL librart. %s.\n", strerror(errno));
      exit(2);
    }

  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  ctx = SSL_CTX_new(TLSv1_client_method());

  if(ctx == NULL)
    {
      fprintf(stderr, "Error initializing SSL context. %s.\n", strerror(errno));
      exit(2);
    }

  ssl = SSL_new(ctx);
  if(ssl == NULL)
    {
      fprintf(stderr, "Error setting up SSL. %s.\n", strerror(errno));
      exit(2);
    }

  if(SSL_set_fd(ssl, sockfd) == 0)
    {
      fprintf(stderr, "Error associating sockfd to SSL. %s.\n", strerror(errno));
      exit(2);
    }

  if(SSL_connect(ssl) == -1)
    {
      fprintf(stderr, "Error connecting. %s.\n", strerror(errno));
      exit(2);
    }

  // write ID to server
  char id_temp[10];
  sprintf(id_temp, "ID=%s\n", id);
  write_to_server(id_temp);
  if(l_flag)
    fprintf(logfile, "ID=%s\n", id);

  // initialize temperature sensor
  mraa_aio_context aio = mraa_aio_init(1);
  if(aio == NULL)
    {
      fprintf(stderr, "Failed to initialize AIO. %s.\n", strerror(errno));
      mraa_deinit();
      exit(2);
    }

  // declare and initialize variables needed
  struct tm *info;
  int rawtemp = 0;
  float temp = 0;

  struct pollfd fdi;
  fdi.fd = sockfd;
  fdi.events = POLLIN;

  char* buf = (char *)malloc(sizeof(char) * 256);
  if(buf == NULL)
    {
      fprintf(stderr, "Fail to malloc. %s.\n", strerror(errno));
      free(buf);
      exit(2);
    }

  struct timeval mytime;
  time_t expected_time = 0;

  // generate report
  while(1)
    {
      gettimeofday(&mytime, NULL);
      if(r_flag && mytime.tv_sec >= expected_time)
	{
	  rawtemp = mraa_aio_read(aio);
	  temp = convert(rawtemp, f_flag);
	  info = localtime(&mytime.tv_sec);
	  char timetime[64];
	  sprintf(timetime, "%02d:%02d:%02d %0.1f\n", info->tm_hour, info->tm_min, info->tm_sec, temp);
	  write_to_server(timetime);
	  if(l_flag)
	    {
	      fprintf(logfile, "%02d:%02d:%02d %0.1f\n", info->tm_hour, info->tm_min, info->tm_sec, temp);
	    }
	  expected_time = mytime.tv_sec + period;
	}

      // get commands from server
      if(poll(&fdi, 1, 0))
        {
          int len = SSL_read(ssl, buf, 256);
	  if(len > 0)
	    {
	      buf[len] = '\0';
	    }

	  int start = 0;
	  int i = 0;
	  while (start < len) {
	    char* input = buf + start;
	    while (i < len && input[i] != '\n') {
	      i++;
	    }
	    input[i+1] = '\0';
	    process(input);
	    start = start + i + 1;
	  }
        }

      if(run_flag == 0)
	break;
    }

  // SHUTDOWN message
  info = localtime(&mytime.tv_sec);
  char timetime[64];
  sprintf(timetime, "%02d:%02d:%02d SHUTDOWN\n", info->tm_hour, info->tm_min, info->tm_sec);
  write_to_server(timetime);
  if(l_flag)
    fprintf(logfile, "%02d:%02d:%02d SHUTDOWN\n", info->tm_hour, info->tm_min, info->tm_sec);

  mraa_aio_close(aio);

  if(l_flag)
    fclose(logfile);

  free(buf);

  exit(0);
}
