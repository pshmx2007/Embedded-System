# ReadMe

Embedded System Mini-Project
2018.04.02 Submitted
YOUTUBE MOV URL : https://youtu.be/2vypEp2Vwi8 동영상은 유투브로도 업로드하였습니다.
Project Title : Danger Bahavior Control on the Traffic System

Project Outline :
  Sometimes people can't recognize red light because of watching their smart phone. And it can be causing of big accident. So, this project maybe prevent these accidents. It stops people when they are going to cross the road with red light by making a sound beep~~~


Project Codes Remarkable Points
  I thought, in this Project it will be important to react immediatley, so polling system, or single thread design is not suitable. i think multi threading or interrupt detecting and calling callbacks is more suitalbe.

  if i use interrupt in echo signal,
   i would added this line
    GPIO.add_event_detect(24, GPIO.FALLING, callback=checkDistance, bouncetime=300)
  but echo signal is not important. because, real important thing is ultra sensor detect the humans. so, i take the multi threading, and there is a timer thing.

  timer = threading.Timer(timeinterval, Timer_Detect)
  this is a timer.

Project Code explains
  Traffic is controled with while loop. in the car traffic,
    red light during 5 seconds
    yellow light during 2 seconds
    green light during 5 seconds

  and in the man's traffic
    red light turned on with car's green light on
    green light turned on with car's red light on
  when ultra sensor detect thing in 10cm range, it calls playsound function
