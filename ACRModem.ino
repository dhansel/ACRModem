// -----------------------------------------------------------------------------
// Modulator/Demodulator for 88-ACR Altair Audio Cassette Interface (300 baud)
// Copyright (C) 2018 David Hansel
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
// -----------------------------------------------------------------------------

#define PIN_SERIAL_IN   2  // must be 2 or 3 (uses interrupt pin INT0 or INT1)
#define PIN_SERIAL_OUT A4  // can be any pin
#define PIN_AUDIO_IN    8  // must be  8 (hardwired ICP1 for timer1)
#define PIN_AUDIO_OUT  11  // must be 11 (hardwired OC2A for timer2)
#define PIN_LED         4  // can be any pin

// timing values for audio output:
// 8Mz main clock with /32 pre-scaler, the output toggles on 
// timer compare match so the timer must count up twice for a full wave:
#define OCRA_1850HZ  67 // 8000000/32/2/1850 = 67.57 = 541 microseconds
#define OCRA_2400HZ  52 // 8000000/32/2/2400 = 52.08 = 417 microseconds

// timing values for audio input
#define PERIOD_MAX_US 2000  // 1000Hz = 2000 microseconds
#define PERIOD_MID_US  471  // 2125Hz =  471 microseconds
#define PERIOD_MIN_US  333  // 3000Hz =  333 microseconds

// number of good audio pulses in a row required before entering "send" mode
#define NUM_GOOD_PULSES 100

// number of bad audio pulses in a row required before exiting "send" mode
// at 300 baud and audio frequencies of 1850 and 2400Hz, one bit is made up
// of 6-8 pulses. So if we just keep outputting the previous bit value then
// one or two bad pulses may not result in a receive error.
#define NUM_BAD_PULSES  3

// are we in "send" mode?
volatile bool send_data = false;

// timer value for current audio output frequency
volatile byte audio_out_timer_value = OCRA_2400HZ;


void got_pulse(bool good)
{
 static volatile byte good_pulses = 0, bad_pulses = 0;

 if( good )
  {
    bad_pulses = 0;
    if( good_pulses<NUM_GOOD_PULSES ) 
      {
        good_pulses++;
        if( good_pulses==NUM_GOOD_PULSES ) 
          { 
            digitalWrite(PIN_LED, HIGH); 
            send_data = true; 
          }
        else
          {
            // enable timer 1 overflow interrupt (checks for too-long pulse)
            TIFR1  |= bit(OCF1A);
            TIMSK1 |= bit(OCIE1A);
          }
      }
  }
 else if( bad_pulses<NUM_BAD_PULSES )
   {
     bad_pulses++;
     if( bad_pulses==NUM_BAD_PULSES )
       {
         good_pulses = 0;    
         if( send_data )
           {
             digitalWrite(PIN_LED, LOW); 
             digitalWrite(PIN_SERIAL_OUT, HIGH);
             send_data = false;
           }

         // disable timer 1 overflow interrupt (only needed while decoding)
         TIMSK1 &= ~bit(OCIE1A);
       }
   }
}


// called when timer 2 reaches its maximum value (compare match) 
// this interrupt only gets enabled when needed
ISR(TIMER2_COMPA_vect)
{
  // set audio out frequency for next wave
  OCR2A = audio_out_timer_value;  

  // disable interrupts for audio output timer (i.e. this ISR)
  TIMSK2 = 0;
}


// called when an edge is detected on the serial input
void serial_in_change()
{
  // reset compare match interrupt flag so we react to interrupts that
  // happen while (but not before) processing the serial input change
  TIFR2 = bit(OCIE2A);

  // read new serial input value
  bool data = digitalRead(PIN_SERIAL_IN);

  // set audio out frequency (wave length) depending on serial input
  audio_out_timer_value = data ? OCRA_2400HZ : OCRA_1850HZ;

  // forward data to pin 1 (external serial out)
  PORTD = (PORTD & ~0x02) | (data ? 0x02 : 0);

  // if the timer counter is smaller than target value (minus a small safety margin)
  // then set the new new target value now, otherwise enable compare match
  // interrupt and set the new target value in the interrupt handler
  if( TCNT2 < audio_out_timer_value-1 ) 
    OCR2A = audio_out_timer_value;
  else 
    TIMSK2 = bit(OCIE2A);
}


// called when an edge is detected on the external serial input
void extern_serial_in_change()
{
  // if not currently converting audio to serial then 
  // forward pin 1 (external serial in) to serial out
  if( !send_data ) digitalWrite(PIN_SERIAL_OUT, PIND & 0x01);
}


// called when a rising edge is detected at the ICP1 input (pin 8) 
ISR(TIMER1_CAPT_vect) 
{ 
  // reset timer 
  TCNT1 = 0;

  // reset output compare A interrupt in case it occurred simultaneously
  TIFR1 = bit(OCF1A);

  // timer ticks equals microseconds since previous rising edge
  // (8Mz clock with prescaler 8 => 1 microsecond per tick)
  unsigned int ticks = ICR1;

  if( ticks<PERIOD_MIN_US )
    got_pulse(false); // too short pulse => bad pulse
  else 
    {
      // detected pulse with PERIOD_MIN_US <= ticks <= PERIOD_MAX_US
      // (we can't get here with ticks>PERIOD_MAX_US
      // because the COMPA interrupt would have been triggered before that)
      static bool p1 = true, p2 = true;
      bool p3 = ticks<PERIOD_MID_US;
      
      if( p1!=p2 && p2!=p3 )
        {
          // of the three most recent pulses, the middle one was different
          // from the first and last => bad pulse (one bit is made up of 
          // about six pulses so there should never be a short->long->short
          // or long->short->long sequence)
          got_pulse(false);
          p2 = p3;
        }
      else
        {
          // previous pulse was good => send it.
          // note that what is being sent out the serial interface is two
          // pulses (or about 1/3 bit) behind the incoming audio: first the
          // incoming audio pulse has to complete before the computer can see
          // its length and then we stay one more pulse behind so we can
          // detect single-pulse errors (see above).
          got_pulse(true);
          if( send_data ) digitalWrite(PIN_SERIAL_OUT, p2);
          p1 = p2; p2 = p3;
        }
    }
}


// called when timer1 reaches its maximum value at PERIOD_MAX_US (2000)
ISR(TIMER1_COMPA_vect)
{  
  // reset timer 
  TCNT1 = 0;

  // reset input capture interrupt in case it occurred simultaneously
  TIFR1 = bit(ICF1);
  ICR1  = 0;
  
  // we haven't seen a rising edge for PERIOD_MAX_US microseconds => bad pulse
  got_pulse(false);
}


void setup() 
{
  // calibration for internal oscillator (not necessary if using crystal)
  // change value such that output square wave on pin 11 is close to 
  // 2400Hz when NOT sending any serial data
  OSCCAL = 0x8e;

  // set up timer2 to produce 1850/2400Hz output wave on OC2A (pin 11)
  // WGM22:20 = 010 => clear-timer-on-compare (CTC) mode 
  // COM2A1:0 =  01 => toggle OC2A on compare match
  // CS22:20  = 011 => CLK/32 prescaler
  TCCR2A = bit(COM2A0) | bit(WGM21);
  TCCR2B = bit(CS21) | bit(CS20);

  // set initial audio output frequency
  audio_out_timer_value = OCRA_2400HZ;
  OCR2A  = audio_out_timer_value;

  // set up serial input to create an interrupt on signal change
  pinMode(PIN_SERIAL_IN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_SERIAL_IN), serial_in_change, CHANGE);

  // set up timer1 to create an input capture interrupt when detecting a 
  // rising edge at the ICP1 input (pin 8) or when reaching PERIOD_MAX_US
  TCCR1A = 0;  
  TCCR1B = bit(ICNC1) | bit(ICES1) | bit(CS11); // enable input capture noise canceler, select rising edge and prescaler 8
  TCCR1C = 0;  
  OCR1A  = PERIOD_MAX_US; // set up for output compare match A after PERIOD_MAX_US microseconds
  TIMSK1 = bit(ICF1); // enable interrupt on input capture

  // set up serial output
  pinMode(PIN_SERIAL_OUT, OUTPUT);
  digitalWrite(PIN_SERIAL_OUT, HIGH);

  // set up audio output
  pinMode(PIN_AUDIO_OUT, OUTPUT);

  // set up LED to signal whether we are receiving good data from audio
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  // set up pin 0/1 for forwarding between main and external serial
  // pin 0 must be connected to pin 3
  pinMode(0, INPUT_PULLUP);
  pinMode(1, OUTPUT);
  digitalWrite(1, HIGH);
  attachInterrupt(digitalPinToInterrupt(3), extern_serial_in_change, CHANGE);

  // disable timer0 overflow interrupt (DISABLES MILLIS() FUNCTION!)
  // necessary so we don't introduce timing errors when forwarding serial data
  // due to timer1 interrupt handling in Arduino core
  TIMSK0 &= ~bit(TOIE0);
}


void loop() 
{/* nothing to do here */}
