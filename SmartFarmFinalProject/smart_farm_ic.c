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
#include <wiringPiSPI.h> 
#include <time.h>


#define DBHOST "18.222.59.249"
#define DBUSER "root"
#define DBPASS "root"
#define DBNAME "project_farm"

#define CS_MCP3208 11
#define SPI_CHANNEL 0 
#define SPI_SPEED 1000000
#define MAX 1
#define MAXTIMINGS 85
#define RED	7 
#define GREEN 9 
#define BLUE 8 
#define FAN	22

static int DHTPIN = 11;
static int dht22_dat[5] = {0,0,0,0,0};
static uint8_t sizecvt(const int read);

MYSQL *connector;
MYSQL_RES *result;
MYSQL_ROW row;

static int read_dht22_dat();
char query[1024];

float temp_buffer[MAX];
int light_buffer[MAX];

int fill_ptr = 0;
int use_ptr = 0;
int count = 0;

float temperature = 0.0f;
unsigned char adcChannel_light = 0;
int adcValue_light = 0;
float vout_light;	
float vout_oftemp;
float percentrh = 0;
float supsiondo = 0;

pthread_cond_t empty, fill;
pthread_mutex_t mutex;

void get();
void put(float tmp, int lht);

void timer_for_data();
void *getSensorData(void* arg);
void *sendSensorData(void* arg);
void *fan_controller(void* arg);
void *LED_controller(void* arg);
void sig_handler(int signo); // 마지막 종료 함수
int read_mcp3208_adc(unsigned char adcChannel);

int main(int argc, char* argv[]){
    signal(SIGINT, (void *)sig_handler);
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
    if(wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED) == -1)
    {
	fprintf(stdout, "wiringPiSPISetup Failed :%s\n", strerror(errno));
	return 1;
    }
	
    pinMode(CS_MCP3208, OUTPUT);
    pinMode(RED, OUTPUT);
    pinMode(GREEN, OUTPUT);
    pinMode(BLUE, OUTPUT);
    pinMode(FAN, OUTPUT) ;

    pthread_t producer, send2server, fan_manager, light_manager;
    pthread_create(&producer, NULL, getSensorData, "prod");
    pthread_create(&send2server, NULL, sendSensorData, "s2s");
    pthread_create(&fan_manager, NULL, fan_controller, "fm");
    pthread_create(&light_manager, NULL, LED_controller, "lm");

    pthread_join(producer, NULL);
    pthread_join(send2server, NULL);
    pthread_join(fan_manager, NULL);
    pthread_join(light_manager, NULL);
    return 0;
}

void *getSensorData(void* arg){
    struct timespec delay_time2Produce;
    while(1){
        pthread_mutex_lock(&mutex);
        while(count == MAX)
            pthread_cond_wait(&empty, &mutex);
        if(read_dht22_dat()){
            put(temperature, adcValue_light);
        }
	delay_time2Produce.tv_sec = 0;
	delay_time2Produce.tv_nsec = 1000;
	pthread_cond_signal(&fill);
        pthread_mutex_unlock(&mutex);
        nanosleep(&delay_time2Produce,NULL);
    }
}

void *sendSensorData(void* arg){
    struct timespec delay_time2SendServer;
    while(1){
        pthread_mutex_lock(&mutex);
        while(count==0)
            pthread_cond_wait(&fill, &mutex);
	get();
	timer_for_data();	
	delay_time2SendServer.tv_sec = 10;
	delay_time2SendServer.tv_nsec = 0;	
        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&mutex);
	nanosleep(&delay_time2SendServer,NULL);
    }
}

void *fan_controller(void* arg){
    struct timespec delay_time2FAN_ON;
    while(1){
	pthread_mutex_lock(&mutex);
        while(count==0)
            pthread_cond_wait(&fill, &mutex);
	get();
	if(temperature > 20.0f)
	{
	    digitalWrite(FAN, 1);
	    delay_time2FAN_ON.tv_sec = 5;
	    delay_time2FAN_ON.tv_nsec = 0;	
        }
	
	pthread_cond_signal(&empty);
        pthread_mutex_unlock(&mutex);
	nanosleep(&delay_time2FAN_ON,NULL);
	digitalWrite(FAN, 0);
	delay_time2FAN_ON.tv_sec = 1;
	nanosleep(&delay_time2FAN_ON,NULL);
    }
}

void *LED_controller(void* arg)
{
    while(1){
        pthread_mutex_lock(&mutex);
        while(count==0)
            pthread_cond_wait(&fill, &mutex);
	get();
	if(adcValue_light > 2500)
	{
	    digitalWrite(RED, 1);
        }
	else
	    digitalWrite(RED, 0);
	pthread_cond_signal(&empty);
        pthread_mutex_unlock(&mutex);
    }
}

void timer_for_data(){
    sprintf(query,"insert into farm_data values (now(),%f,%d)", temperature,adcValue_light);
    mysql_query(connector, query);
    printf("Uploaded to Server\n");
}

void put(float tmp, int lht){
    temp_buffer[fill_ptr] = tmp;
    light_buffer[fill_ptr] = lht;
    fill_ptr = (fill_ptr+1)%MAX;
    count++;
}

void get()
{
    float tmp = temp_buffer[use_ptr];
    int lht = light_buffer[use_ptr];
    use_ptr = (use_ptr +1)%MAX;
    count --;
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
    adcValue_light = read_mcp3208_adc(adcChannel_light);
    printf("Temperature = %.2f *C ,light sensor = %d\n", t, adcValue_light );
    temperature = t;
    return 1;
  }
  else
  {
    //printf("Data not good, skip\n");
    return 0;
  }
}

int read_mcp3208_adc(unsigned char adcChannel) 
{
	unsigned char buff[3];
	int adcValue = 0;
	
	buff[0] = 0x06 | ((adcChannel & 0x07) >> 2);
	buff[1] = ((adcChannel & 0x07) << 6);
	buff[2] = 0x00;
	
	digitalWrite(CS_MCP3208, 0);
	wiringPiSPIDataRW(SPI_CHANNEL, buff, 3);
	
	buff[1] = 0x0f & buff[1];
	adcValue = (buff[1] << 8 ) | buff[2];
	
	digitalWrite(CS_MCP3208, 1);
	
	return adcValue;
}


void sig_handler(int signo)
{
    printf("process stop\n");
	digitalWrite(RED, 0);
	digitalWrite(GREEN, 0);
	digitalWrite(BLUE, 0);
    digitalWrite (FAN, 0) ;
    exit(0);
}

