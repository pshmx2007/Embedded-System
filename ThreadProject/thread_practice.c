#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <mysql/mysql.h>
#include <sys/types.h>
#include <errno.h>
#include <wiringPi.h>
#include <stdint.h>
#include <string.h> 
#include <signal.h> 
#include <softPwm.h>

#define MAX 5
#define MAXTIMINGS 85
#define DBHOST "127.0.0.1"
#define DBUSER "root"
#define DBPASS "root"
#define DBNAME "demofarmdb"

static int DHTPIN = 11;
static int dht22_dat[5] = {0,0,0,0,0};
static uint8_t sizecvt(const int read);

MYSQL *connector;
MYSQL_RES *result;
MYSQL_ROW row;

static int read_dht22_dat();
char query[1024];

int loops = 1000;
float buffer[MAX];
int fill_ptr = 0;
int use_ptr = 0;
int count = 0;
float temperature = 0.0f;

pthread_cond_t empty, fill;
pthread_mutex_t mutex;

float get();
void put(float value);
void *consumer(void* arg);
void *producer(void* arg);

int main(int argc, char* argv[]){
    if (wiringPiSetup () == -1)
    {
	fprintf(stdout, "Unable to start wiringPi: %s\n", strerror(errno));
	return 1 ;
    }

    connector = mysql_init(NULL);
    if (!mysql_real_connect(connector, DBHOST, DBUSER, DBPASS, DBNAME, 3306, NULL, 0))
    {
        fprintf(stderr, "%s\n", mysql_error(connector));
        return 0;
    }
    
    pthread_t prod, cons1, cons2;
    pthread_create(&prod, NULL, producer, "prod");
    pthread_create(&cons1, NULL, consumer, "cons1");
    pthread_create(&cons2, NULL, consumer, "cons2");

    pthread_join(prod, NULL);
    pthread_join(cons1, NULL);
    pthread_join(cons2, NULL);
    return 0;
}

void *producer(void* arg){
    int i;
    while(1){
        pthread_mutex_lock(&mutex);
        while(count == MAX)
            pthread_cond_wait(&empty, &mutex);
        if(read_dht22_dat()){
        	put(temperature);
        	printf("%s putting %.2f\n", (void*)arg, temperature);
        }
	pthread_cond_signal(&fill);
        pthread_mutex_unlock(&mutex);
        sleep(3);
    }
}
void *consumer(void* arg){
    int i;
    while(1){
        pthread_mutex_lock(&mutex);
        while(count==0)
            pthread_cond_wait(&fill, &mutex);
        float tmp = get();
	mysql_query(connector, query);
        printf("%s consume %.2f\n", (void*)arg, tmp);
        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&mutex);
    }
}

void put(float value){
    buffer[fill_ptr] = value;
    fill_ptr = (fill_ptr+1)%MAX;
    count++;
}

float get()
{
    float tmp = buffer[use_ptr];
    use_ptr = (use_ptr +1)%MAX;
    count --;
    return tmp;
}

static uint8_t sizecvt(const int read)
{
  /* digitalRead() and friends from wiringpi are defined as returning a value
  < 256. However, they are returned as int() types. This is a safety function */

  if (read > 255 || read < 0)
  {
    printf("Invalid data from wiringPi library\n");
    exit(EXIT_FAILURE);
  }
  return (uint8_t)read;
}

static int read_dht22_dat()
{
  uint8_t laststate = HIGH;
  uint8_t counter = 0;
  uint8_t j = 0, i;

  dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

  // pull pin down for 18 milliseconds
  pinMode(DHTPIN, OUTPUT);
  digitalWrite(DHTPIN, HIGH);
  delay(10);
  digitalWrite(DHTPIN, LOW);
  delay(18);
  // then pull it up for 40 microseconds
  digitalWrite(DHTPIN, HIGH);
  delayMicroseconds(40); 
  // prepare to read the pin
  pinMode(DHTPIN, INPUT);

  // detect change and read data
  for ( i=0; i< MAXTIMINGS; i++) {
    counter = 0;
    while (sizecvt(digitalRead(DHTPIN)) == laststate) {
      counter++;
      delayMicroseconds(1);
      if (counter == 255) {
        break;
      }
    }
    laststate = sizecvt(digitalRead(DHTPIN));

    if (counter == 255) break;

    // ignore first 3 transitions
    if ((i >= 4) && (i%2 == 0)) {
      // shove each bit into the storage bytes
      dht22_dat[j/8] <<= 1;
      if (counter > 50)
        dht22_dat[j/8] |= 1;
      j++;
    }
  }

  // check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
  // print it out if data is good
  if ((j >= 40) && 
      (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF)) ) {
        float t, h;
	int tint =0;
        h = (float)dht22_dat[0] * 256 + (float)dht22_dat[1];
        h /= 10;
        t = (float)(dht22_dat[2] & 0x7F)* 256 + (float)dht22_dat[3];
        t /= 10.0;
        if ((dht22_dat[2] & 0x80) != 0)  t *= -1;
	tint = t;
    sprintf(query,"insert into t2 values (now(),%f)", t);
    printf("Temperature = %.2f *C \n", t );
    temperature = t;
    return 1;
  }
  else
  {
    printf("Data not good, skip\n");
    return 0;
  }
}



