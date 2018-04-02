import RPi.GPIO as GPIO 
import threading
import time
import pygame

GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)

TRIG = 23
ECHO = 24
CAR_RED = 5
CAR_YEL = 6
CAR_GRE = 13
HU_RED = 19
HU_GRE = 26
WARNING = 21

GPIO.setup(TRIG, GPIO.OUT)
GPIO.setup(ECHO, GPIO.IN)
GPIO.setup(CAR_RED, GPIO.OUT)
GPIO.setup(CAR_YEL, GPIO.OUT)
GPIO.setup(CAR_GRE, GPIO.OUT)
GPIO.setup(HU_RED, GPIO.OUT)
GPIO.setup(HU_GRE, GPIO.OUT)
GPIO.setup(WARNING, GPIO.OUT)

pygame.mixer.init()
sound = pygame.mixer.Sound("beep.wav")
pulse = 0.00001
timeinterval = 0.5
signal = 0
off = False
on = True

def playSound():
    sound.play()
    time.sleep(0.5)



def Timer_Detect():
    GPIO.output(TRIG, False)
    time.sleep(1)
    timer = threading.Timer(timeinterval, Timer_Detect)
    timer.start()
    GPIO.output(TRIG, True)     
    time.sleep(pulse)
    GPIO.output(TRIG, False)
   
    GPIO.wait_for_edge(ECHO, GPIO.RISING)
    pulse_start = time.time()
    GPIO.wait_for_edge(ECHO, GPIO.FALLING)
    pulse_end = time.time()
    pulse_duration = pulse_end - pulse_start
    distance = pulse_duration*17150
    distance = round(distance,2)
    if distance < 10 and signal == 1 :
         playSound()
    print "Distance: ", distance, "cm"

Timer_Detect()

while 1:  
    signal = 0    
    GPIO.output(CAR_RED, on)
    GPIO.output(CAR_YEL, off)
    GPIO.output(CAR_GRE, off)
    GPIO.output(HU_GRE, on)
    GPIO.output(HU_RED, off)
    time.sleep(5)
    GPIO.output(CAR_RED, off)
    GPIO.output(CAR_YEL, on)
    GPIO.output(CAR_GRE, off)
    time.sleep(2)
    signal = 1
    GPIO.output(CAR_RED, off)
    GPIO.output(CAR_YEL, off)
    GPIO.output(CAR_GRE, on)
    GPIO.output(HU_GRE, off)
    GPIO.output(HU_RED, on)
    time.sleep(5)

GPIO.cleanup()

    

