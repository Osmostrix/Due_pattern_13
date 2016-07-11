// Ashish Naik
// December 4, 2015
// Due_pattern_13
// This code is used for programming the microcontroller for a project operating about 80 LEDs and infrared sensors.

/// Pin Assignments:
/// - SPI CLK  = SDA1, A.17, peripheral B -> TLC5940 SCLK (pin 25)
/// - SPI MOSI = TX1 (pin 18), A.11. peripheral A -> TLC5940 SIN (pin 26)
/// - SPI MISO = RX1 (pin 17), A.10, peripheral A -> TLC5940 SOUT (pin 17) [opt]
/// - pin 6 -> TLC5940 VPRG (pin 27)
/// - pin 5 -> TLC5940 XLAT (pin 24)
/// - pin 4 -> TLC5940 BLANK (pin 23)
/// - pin 3 -> TLC5940 GSCLK (pin 18)
/// - pin 2 -> TLC5940 XERR (pin 16)  [optional]

////////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "Arduino.h"
#include "due_tlc5940.h"

#define NUMBER_OF_SHIFT_CHIPS   2

/* Width of data (how many ext lines).
*/
#define DATA_WIDTH   NUMBER_OF_SHIFT_CHIPS * 8

/* Width of pulse to trigger the shift register to read and latch.
*/
#define PULSE_WIDTH_USEC   5

/* Optional delay between shift register reads.
*/
#define POLL_DELAY_MSEC   1

/* You will need to change the "int" to "long" If the
 * NUMBER_OF_SHIFT_CHIPS is higher than 2.
*/
#define BYTES_VAL_T unsigned int

int ploadPin        = 8;  // Connects to Parallel load pin the 165
int clockEnablePin  = 9;  // Connects to Clock Enable pin the 165
int dataPin         = 11; // Connects to the Q7 pin the 165
int clockPin        = 12; // Connects to the Clock pin the 165

// Color definitions for use in ledNode struct
typedef enum COLOR {

      BLUE,
      GREEN,
      RED,
      YELLOW,
      PINK,
      PURPLE,
      ORANGE,
      WHITE,
      BLACK
};

// Short array of enum values to reference each color easily
static COLOR library[9] = {BLUE, GREEN, RED, YELLOW, PINK, PURPLE, ORANGE, WHITE, BLACK};
BYTES_VAL_T l_num = 512;// This is the value of 2^9 in BYTES_VAL_T. Need to adjust this according to LED&Sensor count

// Reference grids filled with PWM values for each LED bulb
// [x][y], where x corresponds to a color, y corresponds to a idNumber
int B[9][9] = {{4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095},/*BLUE*/
               {0,0,0,0,0,0,0,0,0},/*GREEN*/
               {0,0,0,0,0,0,0,0,0},/*RED*/
               {0,0,0,0,0,0,0,0,0},/*YELLOW*/
               {210, 381, 381, 381, 381, 381, 381, 381, 381},/*PINK*/
               {4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095},/*PURPLE*/
               {0,0,0,0,0,0,0,0,0},/*ORANGE*/
               {1023, 2047, 4095, 4095, 4095, 4095, 4095, 4095, 4095},/*WHITE*/
               {0,0,0,0,0,0,0,0,0},/*BLACK*/
              };

int G[9][9] = {{0,0,0,0,0,0,0,0,0},/*BLUE*/
               {4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095},/*GREEN*/
               {0,0,0,0,0,0,0,0,0},/*RED*/
               {666, 2700, 2700, 666, 2700, 2700, 2700, 2700, 2700},/*YELLOW*/
               {200, 200, 200, 200, 200, 200, 200, 200, 200},/*PINK*/
               {0,0,0,0,0,0,0,0,0},/*PURPLE*/
               {127, 255, 255, 127, 255, 255, 255, 255, 255},/*ORANGE*/
               {1023, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095},/*WHITE*/
               {0,0,0,0,0,0,0,0,0},/*BLACK*/
              };

int R[9][9] = {{0,0,0,0,0,0,0,0,0},/*BLUE*/
               {0,0,0,0,0,0,0,0,0},/*GREEN*/
               {4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095},/*RED*/
               {4095, 1200, 1200, 511, 1200, 1200, 1200, 1200, 1200},/*YELLOW*/
               {4095, 1533, 1533, 1533, 1533, 1533, 1533, 1533, 1533},/*PINK*/
               {4095, 511, 511, 511, 511, 511, 511, 511, 511},/*PURPLE*/
               {4095, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023},/*ORANGE*/
               {4095, 4095, 2047, 2047, 2047, 2047, 2047, 2047, 2047},/*WHITE*/
               {0,0,0,0,0,0,0,0,0},/*BLACK*/
              };



// This hold current state information about a particular LED on the grid
// Holds: current color of LED, status as blinking or not, and pointers to adjacent LEDs
typedef struct LedNode {
        int idNumber;
        COLOR current;
        int blinking;
        int off;
        struct LedNode *north;
        struct LedNode *east;
        struct LedNode *south;
        struct LedNode *west;
  
}LedNode;

// Set the initial values of a node
// NOTE: Each node will correlate to a single LED, and therefore 3 channels that lead to the LED.
// Channel numbers are equal to idNumber x 3 = channel_1.
// channel_2 = channel_1+1.
// channel_3 = channel_1+2.
struct LedNode *initializeN(LedNode *subject, int id) {

        subject = (LedNode *)malloc(sizeof(LedNode));
        subject->idNumber = id;
        subject->current = BLACK;
        subject->blinking = 0;
        subject->off = 1;
        subject->north = NULL;
        subject->east = NULL;
        subject->south = NULL;
        subject->west = NULL;
        return subject;
}


// This checks if channel numbers need to be adjusted based on number of TLCs.
// We never use channel 15 of any TLC, so we need to adjust our numbers accordingly.
int channelCheck(int n) {
    int i;
    int variable;
    
    for (i = 1; i < (NUM_TLCS+1); i++) {
         variable = ((i*16)-1);
         if (n >= variable) {
            n++;
         }
         
    }

    return n;
}

// Small recursive function for calculating exponential power equivalent
int power (int x, int y) {

  if (y > 0) {
       x = x*(power(x, y-1));
  }
  else {
    return 1;
  }
  
  return x;
}

// The following functions are used to create the Ripple effect across the board
// Rip is where the effect starts, the remaining functions are design to be activated
// afterwards and use recursion to carry the same effect across the table.
// ADJUST TO GRID SIZE
void Rip(struct LedNode *subject, int color) {
        activateColor(subject, color);
        delay(500);
        deactivateColor(subject);

        struct LedNode *north, *ne, *east, *se, *south, *sw, *west, *nw;

        if (subject->north != NULL) {
            north = subject->north;
            activateColor(north, color);

            if (north->east != NULL) {
                ne = north->east;
                activateColor(ne, color);
            }

            if (north->west != NULL) {
                nw = north->west;
                activateColor(nw, color);
            }
        }

        if (subject->east != NULL) {
            east = subject->east;
            activateColor(east, color);
        }

        if (subject->south != NULL) { 
            south = subject->south;
            activateColor(south, color);

            if (south->east != NULL) {
                se = south->east;
                activateColor(se, color);
            }

            if (south->west != NULL) {
                sw = south->west;
                activateColor(sw, color);
            }
        }

        if (subject->west != NULL) {
            west = subject->west;
            activateColor(west, color);
        }

        delay(500);
        
        deactivateBorder(subject);

        if (nw != NULL) {
            if (nw->west != NULL) {
              nw = nw->west;
  
              if (nw->north != NULL) {
                  nw = nw->north;
                  activateColor(nw, color);
              }
            }
        }
   
        if (north != NULL) {
            if (north->north != NULL) {
                north = north->north;
                activateColor(north, color);
            }
        }

        if (ne != NULL) {
            if (ne->east != NULL) {
                ne = ne->east;
    
                if (ne->north != NULL) {
                    ne = ne->north;
                    activateColor(ne, color);
                }          
            }
        }
        

        if (east != NULL) {
           if (east->east != NULL) {
                east = east->east;
                activateColor(east, color);
              
            }
        }

        if (se != NULL) {
            if (se->east != NULL) {
                se = se->east;
    
                if (se->south != NULL) {
                    se = se->south;
                    activateColor(se, color);
                }
              
            }
        }

        if (south != NULL) {
            if (south->south != NULL) {
                south = south->south;
                activateColor(south, color);
            }
        }

        if (sw != NULL) {
            if (sw->west != NULL) {
                sw = sw->west;
    
                if (sw->south != NULL) {
                    sw = sw->south;
                    activateColor(sw, color);
                }
            }
        }

        
        if (west != NULL) {
            if (west->west != NULL) {
                west = west->west;
                activateColor(west, color);  
            }
        }

}




// Simple function that flashes an chosen LED on and off in the same function.
// Delays are currently a value set before program is run.
// We can modify the delays to be dependent on what program is run later.
void NodeFire(struct LedNode *subject) {
        int relative = (subject->idNumber)*3;
  
        if ((subject->current) == BLUE) {
              activateColor(subject, 0);
              delay(10);

              deactivateColor(subject);
        }

        if ((subject->current) == GREEN) {
              activateColor(subject, 1);
              delay(10);

              deactivateColor(subject);
        }

        if ((subject->current) == RED) {
              activateColor(subject, 2);
              delay(10);

              deactivateColor(subject);
        }

        if ((subject->current) == YELLOW) {
              activateColor(subject, 3);
              delay(10);

              deactivateColor(subject);
        }

        if ((subject->current) == PINK) {
              activateColor(subject, 4);
              delay(10);

              deactivateColor(subject); 
        }

        if ((subject->current) == PURPLE) {
              activateColor(subject, 5);
              delay(10);

              deactivateColor(subject); 
        }

        if ((subject->current) == ORANGE) {
              activateColor(subject, 6);
              delay(10);

              deactivateColor(subject); 
        }
        
        if ((subject->current) == WHITE) {
              activateColor(subject, 7);
              delay(10);

              deactivateColor(subject);
        }

        if ((subject->current) == BLACK) {
              activateColor(subject, 8);
              delay(10);

              deactivateColor(subject);
        }

}

// Basic switch function. LED off becomes on. LED on becomes off.
// The "hold" integer below is used to indicate when this function can be allowed to run in the program.
// This keeps the LED from changing prematurely.
// Can use an array of such integers to manage a matrix of LEDs
static int hold = 0;
static COLOR pocket;
void SwitchUp(struct LedNode *subject) {
        
        if ((subject->off)== 1) {
              if ((subject->current) == BLUE) {
                    activateColor(subject, 0);
              }
      
              if ((subject->current) == GREEN) {
                    activateColor(subject, 1);
              }
      
              if ((subject->current) == RED) {
                    activateColor(subject, 2);
              }

              if ((subject->current) == YELLOW) {
                    activateColor(subject, 3);
              }

              if ((subject->current) == PINK) {
                    activateColor(subject, 4); 
              }

              if ((subject->current) == PURPLE) {
                    activateColor(subject, 5);  
              }

              if ((subject->current) == ORANGE) {
                    activateColor(subject, 6);   
              }

              if ((subject->current) == WHITE) {
                    activateColor(subject, 7);    
              }

              if ((subject->current) == BLACK) {
                    activateColor(subject, 8);    
              }

              subject->off = 0;
              
               
        }
        else {

              deactivateColor(subject);
                 
        }
        
}

void FadeColor(struct LedNode *subject, int color) {

        int relative = channelCheck(((subject->idNumber)*3));
        int current1, current2, current3;
        int standard1, standard2, standard3;

        standard1 = B[color][subject->idNumber];

        standard2 = G[color][subject->idNumber];

        standard3 = R[color][subject->idNumber];

        int i = 0;

        for (i = 0; i < 9; i++) {
            if ((subject->current) == library[i]) {
                  current1 = B[i][subject->idNumber];
                  current2 = G[i][subject->idNumber];
                  current3 = R[i][subject->idNumber];
            }
        } 

        int halt = 0;

        while ((current1 != standard1) || (current2 != standard2) || (current3 != standard3)) {

            if  (current1 != standard1) {
                  if (current1 < standard1) {
                      current1++;
                      setGSData(relative, current1);
                      
                  }
                  else if (current1 > standard1) {
                      current1--;
                      setGSData(relative, current1);
                  }
                  
            }

            
            if  (current2 != standard2) {
                if (current2 < standard2) {
                    current2++;
                    setGSData(relative+1, current2);
                    
                }
                else if (current2 > standard2) {
                    current2--;
                    setGSData(relative+1, current2);
                }
            }
            
            if  (current3 != standard3) {
                if (current3 < standard3) {
                    current3++;
                    setGSData(relative+2, current3);
                    
                }
                else if (current3 > standard3) {
                    current3--;
                    setGSData(relative+2, current3);
                }
            }

            delay(10);
            sendGSData();
        }

        subject->current = library[color];

        if (subject->current == BLACK) {
          subject->off = 1;
        }

        Serial.print("Fade Complete \n");
  
}

boolean pinCheck(int check, int pinValues) {
    int sum = ((l_num-1) - check);

    if (sum ==  pinValues) {
        return true;
    }

    return false;
}

void deactivateBorder(struct LedNode *subject) {
     struct LedNode *jump1 = subject->north;
     struct LedNode *jump2 = subject->south;
     
     deactivateColor(subject->north);
     deactivateColor(jump1->east);
     deactivateColor(subject->east);
     deactivateColor(jump2->east);
     deactivateColor(subject->south);
     deactivateColor(jump2->west);
     deactivateColor(subject->west);
     deactivateColor(jump1->west);
}

// This deactivates any selected LED
void deactivateColor(struct LedNode *subject) {
     int relative = channelCheck(((subject->idNumber)*3));
  
     setGSData((relative), 0);
     setGSData((relative+1), 0);
     setGSData((relative+2), 0);
     sendGSData();
     subject->off = 1;
  
}

// This function activates a particular LED to the indicated color 
void activateColor(struct LedNode *subject, int color) {

  if (subject != NULL) {
      int relative = channelCheck(((subject->idNumber)*3));
      
      if (color == 0) {
            setGSData((relative), B[color][subject->idNumber]);
            setGSData((relative+1), G[color][subject->idNumber]);
            setGSData((relative+2), R[color][subject->idNumber]);
            sendGSData();
                    
      }

      if (color == 1) {
            setGSData((relative), B[color][subject->idNumber]);
            setGSData((relative+1), G[color][subject->idNumber]);
            setGSData((relative+2), R[color][subject->idNumber]);
            sendGSData();
            
      }

      if (color == 2) {
            setGSData((relative), B[color][subject->idNumber]);
            setGSData((relative+1), G[color][subject->idNumber]);
            setGSData((relative+2), R[color][subject->idNumber]);
            sendGSData();
            
      }

      if (color ==  3) {
            setGSData((relative), B[color][subject->idNumber]);
            setGSData((relative+1), G[color][subject->idNumber]);
            setGSData((relative+2), R[color][subject->idNumber]);
            sendGSData();
            
      }

      if (color == 4) {
            setGSData((relative), B[color][subject->idNumber]);
            setGSData((relative+1), G[color][subject->idNumber]);
            setGSData((relative+2), R[color][subject->idNumber]);
            sendGSData();
      }

      if (color == 5) {
            setGSData((relative), B[color][subject->idNumber]);
            setGSData((relative+1), G[color][subject->idNumber]);
            setGSData((relative+2), R[color][subject->idNumber]);
            sendGSData();
      }

      if (color == 6) {
            setGSData((relative), B[color][subject->idNumber]);
            setGSData((relative+1), G[color][subject->idNumber]);
            setGSData((relative+2), R[color][subject->idNumber]);
            sendGSData();
        
      }

      if (color == 7) {
            setGSData((relative), B[color][subject->idNumber]);
            setGSData((relative+1), G[color][subject->idNumber]);
            setGSData((relative+2), R[color][subject->idNumber]);
            sendGSData();
            
      }

      if (color == 8) {
            setGSData((relative), B[color][subject->idNumber]);
            setGSData((relative+1), G[color][subject->idNumber]);
            setGSData((relative+2), R[color][subject->idNumber]);
            sendGSData();
      }
      
      subject->off = 0;
      subject->current = library[color];
  }
}

BYTES_VAL_T pinValues;
BYTES_VAL_T oldPinValues;

/* This function is essentially a "shift-in" routine reading the
 * serial Data from the shift register chips and representing
 * the state of those pins in an unsigned integer (or long).
*/
BYTES_VAL_T read_shift_regs()
{
    long bitVal;
    BYTES_VAL_T bytesVal = 0;

    /* Trigger a parallel Load to latch the state of the data lines,
    */
    digitalWrite(clockEnablePin, HIGH);
    digitalWrite(ploadPin, LOW);
    delayMicroseconds(PULSE_WIDTH_USEC);
    digitalWrite(ploadPin, HIGH);
    digitalWrite(clockEnablePin, LOW);

    /* Loop to read each bit value from the serial out line
     * of the SN74HC165N.
    */
    for(int i = 0; i < DATA_WIDTH; i++)
    {
        bitVal = digitalRead(dataPin);

        /* Set the corresponding bit in bytesVal.
        */
        bytesVal |= (bitVal << ((DATA_WIDTH-1) - i));

        /* Pulse the Clock (rising edge shifts the next bit).
        */
        digitalWrite(clockPin, HIGH);
        delayMicroseconds(PULSE_WIDTH_USEC);
        digitalWrite(clockPin, LOW);
    }

    return(bytesVal);
}

/* Dump the list of zones along with their current status.
*/
void display_pin_values()
{
    Serial.print("Pin States:\r\n");

    for(int i = 0; i < DATA_WIDTH; i++)
    {
        Serial.print("  Pin-");
        Serial.print(i);
        Serial.print(": ");

        if((pinValues >> i) & 1)
            Serial.print("HIGH");
        else
            Serial.print("LOW");

        Serial.print((pinValues >> i));
        
        Serial.print("\r\n");
    }

    Serial.print("\r\n");
}

////////////////////////////////////////////////////////////////////////////////
/// The setup() method runs once, when the sketch starts

void setup ()
 {
  // This function is used to start the TLC5940 chips
  initTLC5940();

  // This starts the feed of serial data
  Serial.begin(9600);

     
    // Intialize pins for communication with the Shift Registers
    pinMode(ploadPin, OUTPUT);
    pinMode(clockEnablePin, OUTPUT);
    pinMode(clockPin, OUTPUT);
    pinMode(dataPin, INPUT);

    digitalWrite(clockPin, LOW);
    digitalWrite(ploadPin, HIGH);

    // Read in and display the pin states at startup.
    
    pinValues = read_shift_regs();
    display_pin_values();
    oldPinValues = pinValues;
    
  return;
 }


////////////////////////////////////////////////////////////////////////////////
/// The loop() method runs over and over again, as long as the Arduino has power


void loop()
 {

  // Initialize the double pointer that will be used to hold the array of nodes
   int k = 9;
   struct LedNode **exstar = (struct LedNode **)malloc((k*sizeof(struct LedNode *)));

  // This array is used to help calculate change in serial data from each individual sensor.
  // Holds the difference resulting from each sensor's input. Changes according to number of LEDs in grid
  int pinExclusion[9]; // IMPORTANT: Always specify the size of this array with a number input directly. NO VARIABLES
                       // Failure to do so will cause unpredicatable anomalies in color output
                       
  // Initialize each individual LED node by allocating memory to them and setting their properties.
  // Calculate the value of each pinExclusion value and store in the array
   for (k = 0; k < 9; k++) {
        exstar[k] = initializeN(exstar[k], k);
        pinExclusion[k] = power(2,k); 
   }

   // This "for" loop connects the different nodes to any adjacent nodes for the sake of making patterns easier.
   // ADJUST TO GRID SIZE
   for (k = 0; k < 9; k++) {

        if (((k%3) < 2)) {
          exstar[k]->west = exstar[k+1];
        }

        if (k < 6) {
            exstar[k]->north = exstar[k+3];
        }

        if (k > 2) {
            exstar[k]->south = exstar[k-3];
        }

        if (k > 0) {
            if (((k%3) != 0)) {
              exstar[k]->east = exstar[k-1];
            }
        }
    
   }

    int i = 0; // Counts number of turns game runs
    int j = 0;// extra variable for double for-loop structures
    

   // Code written from here to the end of the loop varies depending on effects desired for display

   // This activates all programmed colors on each LED in order of how they are wired into the grid
   for (i = 0 ; i < 9; i++) {
        for (j = 0; j < 8 ; j++) {
            activateColor(exstar[i],j);
            delay(500);
            deactivateColor(exstar[i]);
        }
   }

   // An example of the Rip class to show the ripple effect it creates
   Rip(exstar[4], 2);

   delay(100);

   // Get the grid to display a single uniform color
   for (i = 0; i < 9; i++) {
            activateColor(exstar[i], 0);
   }
   
   // NOTE: Array "library" holds COLOR values from 0-8.
   // Order: Blue, Green, Red, Yellow, Pink, Purple, Orange, White, Black
   // NOTE: Small changes in brightness seem to occur with Yellow and Pink
   
  i = 0;

   // For the duration of 6 pin reads, either activate or deactivate LEDs o the grid.
   while (i < 12) {
   /* Read the state of all zones.
    */
          pinValues = read_shift_regs();
          
    /* If there was a change in state, display which ones changed.
    */
          if(pinValues != oldPinValues)
          {
             
              display_pin_values();
              oldPinValues = pinValues;

              // Use "pinCheck" when the sensor read changes.
              // Determine which sensor was triggered
              // Switch that LED
                  
              if (pinCheck(pinExclusion[0], pinValues)) {
                  SwitchUp(exstar[0]);
              }

              if (pinCheck(pinExclusion[1], pinValues)) {
                  SwitchUp(exstar[1]);
              }

              if (pinCheck(pinExclusion[2], pinValues)) {
                  SwitchUp(exstar[3]);
              }

              if (pinCheck(pinExclusion[3], pinValues)) {
                  SwitchUp(exstar[2]);
              }

              if (pinCheck(pinExclusion[4], pinValues)) {
                  SwitchUp(exstar[4]);
              }

              if (pinCheck(pinExclusion[5], pinValues)) {
                  SwitchUp(exstar[5]);
              }

              if (pinCheck(pinExclusion[6], pinValues)) {
                  SwitchUp(exstar[6]);
              }

              if (pinCheck(pinExclusion[7], pinValues)) {
                 SwitchUp(exstar[7]);
              }

              if (pinCheck(pinExclusion[8], pinValues)) {
                  SwitchUp(exstar[8]);
              }
   
               
              
              i++;
          }
    }
    Serial.print("Value Reset \n");// This just tells us the loop ended
    

  
    // Free the allocated memory for each node
    for (k = 0; k<9; k++) {
        free(exstar[k]);
      
    }

    // Free the memory for the double pointer.
    free(exstar);


    // If you wish to restart from the beginning of this loop, activate a sensor, otherwise, simply turn off the setup
    i = 1;
    Serial.print("Restart?\n");
    while (i > 0 ){
          pinValues = read_shift_regs();
          if(pinValues != oldPinValues)
          {
              
              
              oldPinValues = pinValues;

              // The "hold" variable acts as a semaphore and excludes unnecessary sensor input.
              
              i--;
            
          }
      
    }
    
    delay(POLL_DELAY_MSEC);
  //Lamp Test
 }

// A debugging function used for testing when LEDs are connected properly.
void LampTest()
{
  int cnt = 0;
  //Lamp Test
  for(int i = 0; i < 48; i++)
  {
      
    setGSData(i, 4095);
    delay(10);
    sendGSData();
    
    setGSData(i, 0);
    delay(10);
    sendGSData();
    cnt++;

    if(cnt == 15) 
     {
      cnt = 0;
      i++;
     }
  }
  
}

////////////////////////////////////////////////////////////////////////////////
/// End of Code
