#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

/*
 * Simple time measurement tool that returns elapsed time
 * to run another command (passed in arguments).
 */
int main(int argc, char** argv) {
  if (argc<2) {
    fprintf(stderr,"Needs program to execute.\n"); exit(1);
  }
  
	struct timeval t1, t2;
  gettimeofday(&t1, NULL);
  
  pid_t f=fork();
  switch (f) {
    case -1: fprintf(stderr,"Fork failed.\n"); exit(1);
    case 0: execvp(argv[1], &(argv[1]));
    default: waitpid(f,NULL,0);
  }
  gettimeofday(&t2, NULL);
  long delta = (t2.tv_sec-t1.tv_sec)*1000000+(t2.tv_usec-t1.tv_usec);
	printf("Running time: %ld ms\n",delta / 1000);
	return 0;
}
