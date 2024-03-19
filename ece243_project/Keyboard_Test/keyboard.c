// from lecture, trying out for neater code
struct audio_t {
	volatile unsigned int control;
	volatile unsigned char rarc;
	volatile unsigned char ralc;
	volatile unsigned char warc;
	volatile unsigned char walc;
    volatile unsigned int ldata;
	volatile unsigned int rdata;
};

struct timer{
    volatile unsigned int status;
    volatile unsigned int control;
    volatile unsigned int start_low_value;
    volatile unsigned int start_high_value;
    volatile unsigned int count_low;
    volatile unsigned int count_high;
};

/*
PS/2 Port Description (port 1 start: 0xFF200100, port 2 start: 0xFF200108)
for both, it is the data register then the control register
256-byte FIFO
address: Data Register:
bits 0-7 -> data (readable and writeable!)
bit 15 -> R_VALID (if 1, then we get the head of the data FIFO)
bits 16-31 -> R_AVAIL (displays number of entries in the FIFO, reading decreases it by 1)

(address + 4): Control Register
bit 0: RE (enable interrupts for when R_AVAIL > 0, ie when FIFO has data)
bit 8: RI (when interrupt is pending, RI = 1. reset by emptying the FIFO)
bit  10: CE (shows if an error has occurred when sending a command TO the PS/2)
*/

struct ps2{
    volatile unsigned char data; // 8 bits of data
    volatile unsigned char R_VALID;
    volatile unsigned short int R_AVAIL;
    volatile unsigned int control;
};

struct audio_t *const audio_ptr = ((struct audio_t*)0xff203040);
struct timer *const timer_ptr = ((struct timer*)0xFF202000);
struct ps2 *const keyboard1_ptr = ((struct ps2*) 0xFF200100);
struct ps2 *const keyboard2_ptr = ((struct ps2*) 0xFF200108);
volatile int *switch_ptr = (0xFF200040);
volatile int *LEDs = 0xff200000;
volatile int *HEX_0 = 0xFF200020;
volatile int *HEX_5 = 0xFF200030;

const short int KEYCODE[4] = {
    0x1D, // W
    0x1B, // S
    0x1C, // A
    0x23 // D
};

const short int BR_CODE = 0xF0;

int check_key_direction(int* direction, short int key){
    for(int k = 0; k < 4; k++){
        if(key == KEYCODE[k]){
            // we are touching a key we care about! return its corresponding slot/direction
            return k;
        }
    }
    // we didnt touch a key we care about
    return -1;
}

int main(void){
    short int last_input, current_input;
    short int breaking = 0;
    int direction_1[5]; // store player 1 directional inputs, up down left right in THAT ORDER
    int key_state = 0;
    int LED_Out = 0;

    keyboard1_ptr->data = 0xF4;
    keyboard1_ptr->data = 0xFF;

    while(1){
        LED_Out = 0;

        if(keyboard1_ptr->R_AVAIL){
            // there is data available
            current_input = keyboard1_ptr->data;

            if(current_input == BR_CODE){
                // breaking key start
                breaking = 1;
                LED_Out += 0x300;
            }else if(breaking){
                // breaking key end point, actually "let go" of key
                /*key_state = check_key_direction(direction_1, current_input);
                if(key_state != -1){
                    direction_1[key_state] = 0;
                }
                */
                
                for(int k = 0; k < 4; k++){
                    if(current_input == KEYCODE[k]){
                        // we are letting go of a key we care about! update the directions based on this
                        direction_1[k] = 0;
                    }
                }
                breaking = 0;
            }else{
                // normal input, check if it is an input we care about
                /*
                key_state = check_key_direction(direction_1, current_input);
                if(key_state != -1){
                    direction_1[key_state] = 1;
                }
                */
               for(int k = 0; k < 4; k++){
                    if(current_input == KEYCODE[k]){
                        // we are letting go of a key we care about! update the directions based on this
                        direction_1[k] = 1;
                    }
                }
            }
        }
        // output data onto LEDS
        
        for(int k = 0; k < 4; k++){
            if(direction_1[k]){
                LED_Out += 0x1 << k;
            }
        }
        *LEDs = LED_Out;
    }

}