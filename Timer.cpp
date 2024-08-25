#include "Timer.h"

static const char *prefix[] = { "","Kilo","Mega","Giga","Tera","Peta","Hexa" };


#include <sys/time.h>
#include <unistd.h>
#include <string.h>
time_t Timer::tickStart;

void Timer::Init() {

  tickStart=time(NULL);
}

double Timer::get_tick() {

    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)(tv.tv_sec - tickStart) + (double)tv.tv_usec / 1e6;
}

uint32_t Timer::getSeed32() {
  return ::strtoul(getSeed(4).c_str(),NULL,16);
}

uint32_t Timer::getPID() {

  return (uint32_t)getpid();

}

std::string Timer::getSeed(int size) {

  std::string ret;
  char tmp[3];
  unsigned char *buff = (unsigned char *)malloc(size);


  FILE *f = fopen("/dev/urandom","rb");
  if(f==NULL) {
    printf("Failed to open /dev/urandom %s\n", strerror( errno ));
    exit(1);
  }
  if( fread(buff,1,size,f)!=size ) {
    printf("Failed to read from /dev/urandom %s\n", strerror( errno ));
    exit(1);
  }
  fclose(f);


  for (int i = 0; i < size; i++) {
    sprintf(tmp,"%02X",buff[i]);
    ret.append(tmp);
  }
  
  free(buff);
  return ret;

}


std::string Timer::getResult(char *unit, int nbTry, double t0, double t1) {

  char tmp[256];
  int pIdx = 0;
  double nbCallPerSec = (double)nbTry / (t1 - t0);
  while (nbCallPerSec > 1000.0 && pIdx < 5) {
    pIdx++;
    nbCallPerSec = nbCallPerSec / 1000.0;
  }
  sprintf(tmp, "%.3f %s%s/sec", nbCallPerSec, prefix[pIdx], unit);
  return std::string(tmp);

}

void Timer::printResult(char *unit, int nbTry, double t0, double t1) {

  printf("%s\n", getResult(unit, nbTry, t0, t1).c_str());

}

int Timer::getCoreNumber() {

  return (size_t)sysconf(_SC_NPROCESSORS_ONLN);

}

void Timer::SleepMillis(uint32_t millis) {

  usleep(millis*1000);

}

std::string Timer::getTS() {

  std::string ret;
  time_t now = time(NULL);
  char *time = ctime(&now);

  if(time[8]==' ') time[8]='0';
  ret.push_back(time[8]);
  ret.push_back(time[9]);
  ret.push_back(time[4]);
  ret.push_back(time[5]);
  ret.push_back(time[6]);
  ret.push_back(time[22]);
  ret.push_back(time[23]);
  ret.push_back('_');
  ret.push_back(time[11]);
  ret.push_back(time[12]);
  ret.push_back(time[14]);
  ret.push_back(time[15]);
  ret.push_back(time[17]);
  ret.push_back(time[18]);

  return ret;

}