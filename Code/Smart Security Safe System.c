#define F_CPU 8000000UL
#define PASS_LEN 4
#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>

char keypad_map[4][4] =
{
{'1','2','3','A'},
{'4','5','6','B'},
{'7','8','9','C'},
{'*','0','#','D'}
};

char EEMEM pass[PASS_LEN] = {'0','0','0','0'}; //the default pass
char input[PASS_LEN];
char tries = 0; //it the number of wrong tries
char auth = 0; //check if user allowed if he enter the pass or use the key he will be allowed and the alarm dosen't work
char alarm_switch = 1; //this variable store switch mode
char alarm_latched = 0; //Maintains general alert status
char vibration_ok = 0; //the transmission and breakage protection mode is activated
char vibration_latched = 0; //Maintains vibration alert status
volatile uint16_t counter = 0; //A counter used to count time
volatile uint8_t flag = 0; //Flag for the expiry of the time limit

ISR(TIMER1_COMPA_vect) {// In this function the system checks whether 20 seconds have passed and the user has not completed entering the password
	counter++;
	if (counter >= 20) {  
	counter = 0;
	flag = 1;
}
}

void timer(){// This function passes through the input array (in which the system stores the characters that the user presses) and clears it completely
	for(char j = 0; j < PASS_LEN; j++)
	input[j] = 0;
}

void Timer_Start() {//Timer turn on function
	counter = 0;
	flag = 0;
	TCCR1B |= (1 << CS12) | (1 << CS10); 
}

void Timer_Stop() {//Timer turn off function
	TCCR1B &= ~(1 << CS12) & ~(1 << CS10) & ~(1 << CS11); 
	counter = 0;
	flag = 0;
}

void Timer_Init() {//This function is called once at the beginning of the program to initialize the timer
	TCCR1B |= (1 << WGM12);
	OCR1A = 7811;                 
	TIMSK |= (1 << OCIE1A);
	sei();
}

char keypad_GetKey()
{ //Returns the key pressed in the keyboard
	uint8_t row, col;
	for(row = 0; row < 4; row++)
	{
		PORTA = (PORTA & 0xF0) | (0x0F & ~(1 << row));
		_delay_us(10);
		
		for(col = 0; col < 4; col++)
		{
			if(!(PINA & (1 << (col + 4))))
			{
				_delay_ms(20);
				while(!(PINA & (1 << (col + 4))));
				return keypad_map[row][col];
			}
		}
	}
	return 0;
}

void save_pass(char *eeprom_pass){ //save pass in the EEPROM
	eeprom_write_block((const void*)eeprom_pass, (void*)pass, PASS_LEN);
}

void update_pass(char *eeprom_pass){//save pass in the EEPROM put if pass is old self doesn't keep it
	eeprom_update_block((const void*)eeprom_pass, (void*)pass, PASS_LEN);
}

void beep()
{ // function it make keys sound
    PORTC |= (1 << PC1);
    _delay_ms(50);
    PORTC &= ~(1 << PC1);
}

void wrong_beep()
{ // function it make the error sound
    char i;
    for(i = 0; i < 2; i++)
    {
        PORTC |= (1 << PC1);
        _delay_ms(100);
        PORTC &= ~(1 << PC1);
        _delay_ms(100);
    }
}

void alarm_on()
{// function it make the alarm sound
    PORTC |= (1 << PC1);
}

void alarm_off()
{// function it turn off the alarm sound
    PORTC &= ~(1 << PC1);
}

char switch_ok()
{ //function check key state
    if(!(PINC & (1 << PC2)))
    {
        return 1;
    }
    return 0;
}

char AlarmSwitch()
{// function it check if the door opened
    if(!(PINC & (1 << PC3))) return 1;
    return 0;
}

char VibretionSensor(char vib) {// function it check if the safe Move or break opened
	if(vib == 1) {
		if(!(PINC & (1 << PC4)) || !(PIND & (1 << PD2)) || !(PIND & (1 << PD3)) || !(PIND & (1 << PD4))) {
			return 1;
		}
	}
	return 0;
}

char get_input(void)
{// function it take input from user
    char i = 0;
    char key;
    char show_pass = 0;
    while(i < PASS_LEN)
    { //there check if spent 20 seconds in the stage of get input in order to return again to menue
		 if (flag == 1) {
			 flag = 0;
			 return 3;       
		 }
		 
        key = keypad_GetKey();
        if(switch_ok()) return 1;
		if(vibration_ok == 1 && VibretionSensor(1))
			vibration_latched = 1;
		if(vibration_latched)
			alarm_on();
        char sw_alarm_inner = AlarmSwitch();
        if(!alarm_latched && !sw_alarm_inner && alarm_switch && !auth)
            alarm_latched = 1;
        if(sw_alarm_inner && !alarm_switch)
            auth = 0;
        alarm_switch = sw_alarm_inner;
        if(alarm_latched)
            alarm_on();
        if(key)
        {
            if(key == '*')
            {// Here it is ensured that the user pressed * in order to hide or unhide the password from the LCD
                show_pass = !show_pass;
                LCD_MoveCursor(2, 1);
                for(char j = 0; j < i; j++)
                {
                    if(show_pass)
                        LCD_SendChar(input[j]);
                    else
                        LCD_SendChar('*');
                }
                for(char j = i; j < PASS_LEN; j++)
                    LCD_SendChar(' ');
                _delay_ms(300);
            }
            else if(key == '#')
            {//Here it is ensured that the user pressed # to delete one degit
                if(i > 0)
                {
                    i--;
                    LCD_MoveCursor(2, i + 1);
                    LCD_SendChar(' ');
                    LCD_MoveCursor(2, i + 1);
                    beep();
                }
            }
            else if(key == 'B')
            {//Here it is ensured that the user pressed B to back to menue
                for(char j = 0; j < PASS_LEN; j++)
                    input[j] = 0;
                return 2;
            }
            else if(key != 'A' && key != 'C')
            { 
                beep();
                input[i] = key;
                LCD_MoveCursor(2, i + 1);
                if(show_pass)
                    LCD_SendChar(key);
                else
                    LCD_SendChar('*');
                i++;
            }
            _delay_ms(150);
        }
    }
    return 0;
}

char check_password()
{
	char i;
	char temp_pass[PASS_LEN];
	eeprom_read_block((void*)temp_pass, (const void*)pass, PASS_LEN);
	
	for(i = 0; i < PASS_LEN; i++)
	{
		if(input[i] != temp_pass[i]) return 0;
	}
	
	for(i = 0; i < PASS_LEN; i++) input[i] = 0;
	
	return 1;
}

void ChangePassword()
{// it change pass from EEPROM
	eeprom_update_block((const void*)input, (void*)pass, PASS_LEN);
}

void Enable(void)
{// An electrical pulse is sent to the LCD screen to force it to read and display the current data
    PORTD |= (1 << PD0);
    _delay_ms(2);
    PORTD &= ~(1 << PD0);
    _delay_ms(2);
}

void LCD_SendChar(char data)
{// it print char on LCD
    PORTB = data;
    PORTD |= (1 << PD1);
    Enable();
}

void LCD_Send_CMD(char CMD)
{// send CMD to LCD
    PORTB = CMD;
    PORTD &= ~(1 << PD1);
    Enable();
    _delay_ms(1);
}

void LCD_ClrarScreen()
{//clear LCD screen
    LCD_Send_CMD(0x01);
    _delay_ms(10);
}

void LCD_init(void)
{
    DDRB = 0xff; // pin D0 to D7 in LCD
    DDRD = 0x03; //pin 0 is the pin E in LCD and pin 1 is pin RS in LCD
    LCD_Send_CMD(0x38);
    LCD_Send_CMD(0x0E);
    LCD_Send_CMD(0x01);
    _delay_ms(10);
    LCD_Send_CMD(0x06);
}

void LCD_SendString(char *ptr)
{// it print string on LCD
    while(*ptr != 0)
    {
        LCD_SendChar(*ptr);
        ptr++;
    }
}

void LCD_MoveCursor(char row, char col)
{// Move the Cursor to the row and column you want
    char data;
    if(row < 1 || row > 2 || col < 1 || col > 16)
    {
        data = 0x80;
    }
    else if(row == 1)
    {
        data = 0x80 + col - 1;
    }
    else if(row == 2)
    {
        data = 0xC0 + col - 1;
    }
    LCD_Send_CMD(data);
}

int main(void)
{
    LCD_init();
    _delay_ms(150);
	
	Timer_Init();
	
    DDRA = 0x0F; //keypad
    PORTA = 0x00;
	
    DDRC = 0x03; //pin 0 the lock , pin 1 the buzzer , pin 2 the key lock , pin 3 the alarm switch and pin 4 the vibration sensor 
    PORTC = 0xFC;
	
	PORTD |= (1 << PD2);// vibration sensor
	PORTD |= (1 << PD3);// vibration sensor
	PORTD |= (1 << PD4);// vibration sensor
	DDRD  |= (1 << PD5);// LCD turn off
	PORTD |= (1 << PD5);
	
    while(1)
    {
		if(vibration_ok == 1 && VibretionSensor(1))
			vibration_latched = 1;
		if(vibration_latched)
			alarm_on();
		
		if (vibration_ok == 1) {
			PORTC |= (1 << PC6);  
		} else {
			PORTC &= ~(1 << PC6); 
		}
		
        char sw_alarm = AlarmSwitch();
        if(!alarm_latched && !sw_alarm && alarm_switch && !auth)
        {
            alarm_latched = 1;
        }
        if(sw_alarm && !alarm_switch)
        {
            auth = 0;
        }
        alarm_switch = sw_alarm;
        if(alarm_latched)
        {
            alarm_on();
        }
        if(switch_ok())
        {
            if(alarm_latched)
            {
                alarm_latched = 0;
            }
            if(vibration_latched)
            {
	            vibration_latched = 0;
            }
            if(!alarm_latched && !vibration_latched)
            {
	            alarm_off();
            }
            auth = 1;
            LCD_ClrarScreen();
            LCD_SendString("Checked by Key");
            _delay_ms(500);
            LCD_ClrarScreen();
            tries = 0;
            while(switch_ok());
            _delay_ms(50);
			alarm_switch = AlarmSwitch();
			auth = 0;
            continue;
        }
        char key = keypad_GetKey();
        if(!key)
        {
//------------------------------The menue------------------------------
            while(!key && !switch_ok())
            {
               for(char i=0 ; i < 3 ; i++){
				    LCD_MoveCursor(1,1);
				    LCD_SendString("Open safe: A and");
				    LCD_MoveCursor(2,1);
				    LCD_SendString("key or pass");
				    for(int i = 0; i < 50; i++)
				    {
					    key = keypad_GetKey();
					    if(key || switch_ok()) break;
					    _delay_ms(50);
					    char sw_alarm_inner = AlarmSwitch();
					    if(!alarm_latched && !sw_alarm_inner && alarm_switch && !auth)
					    alarm_latched = 1;
					    if(sw_alarm_inner && !alarm_switch)
					    auth = 0;
					    alarm_switch = sw_alarm_inner;
					    if(alarm_latched)
					    alarm_on();
					    if(vibration_ok == 1 && VibretionSensor(1))
					    vibration_latched = 1;
					    if(vibration_latched)
					    alarm_on();
				    }
				    if(key || switch_ok()) break;
				    LCD_ClrarScreen();
				    LCD_MoveCursor(1,1);
				    LCD_SendString("Or press C to");
				    LCD_MoveCursor(2,1);
				    LCD_SendString("change pass");
				    for(int i = 0; i < 50; i++)
				    {
					    key = keypad_GetKey();
					    if(key || switch_ok()) break;
					    _delay_ms(50);
					    char sw_alarm_inner = AlarmSwitch();
					    if(!alarm_latched && !sw_alarm_inner && alarm_switch && !auth)
					    alarm_latched = 1;
					    if(sw_alarm_inner && !alarm_switch)
					    auth = 0;
					    alarm_switch = sw_alarm_inner;
					    if(alarm_latched)
					    alarm_on();
					    if(vibration_ok == 1 && VibretionSensor(1))
					    vibration_latched = 1;
					    if(vibration_latched)
					    alarm_on();
				    }
				    
				    LCD_MoveCursor(1,1);
				    LCD_SendString("Or press D to");
				    LCD_MoveCursor(2,1);
				    LCD_SendString("protection mode");
				    for(int i = 0; i < 50; i++)
				    {
					    key = keypad_GetKey();
					    if(key || switch_ok()) break;
					    _delay_ms(50);
					    char sw_alarm_inner = AlarmSwitch();
					    if(!alarm_latched && !sw_alarm_inner && alarm_switch && !auth)
					    alarm_latched = 1;
					    if(sw_alarm_inner && !alarm_switch)
					    auth = 0;
					    alarm_switch = sw_alarm_inner;
					    if(alarm_latched)
					    alarm_on();
					    if(vibration_ok == 1 && VibretionSensor(1))
					    vibration_latched = 1;
					    if(vibration_latched)
					    alarm_on();
				    }
				    if(key || switch_ok()) break;
				    LCD_ClrarScreen();
				    LCD_MoveCursor(1,1);
				    LCD_SendString("If you want back press B");
				    LCD_MoveCursor(2,1);
				    LCD_SendString("press B");
				    for(int i = 0; i < 50; i++)
				    {
					    key = keypad_GetKey();
					    if(key || switch_ok()) break;
					    _delay_ms(50);
					    char sw_alarm_inner = AlarmSwitch();
					    if(!alarm_latched && !sw_alarm_inner && alarm_switch && !auth)
					    alarm_latched = 1;
					    if(sw_alarm_inner && !alarm_switch)
					    auth = 0;
					    alarm_switch = sw_alarm_inner;
					    if(alarm_latched)
					    alarm_on();
					    if(vibration_ok == 1 && VibretionSensor(1))
					    vibration_latched = 1;
					    if(vibration_latched)
					    alarm_on();

				    }
				    if(key || switch_ok()) break;
			   }
			    PORTD &= ~(1 << PD5); //it turn off LCD aftel menue rebeat 3 times
            }
        }
        if(key)
        {
			PORTD |= (1 << PD5);
            beep();
            switch(key)
            {
//------------------------------Case A if you want open the safe------------------------------			
                case 'A':
                {
					
					Timer_Start();
					LCD_ClrarScreen();
					LCD_MoveCursor(1,1);
					LCD_SendString("Enter pass:");
					LCD_MoveCursor(2,1);
					char result = get_input();

					if (result == 3) {          
					Timer_Stop();
					timer();          
				}
				else if(result == 2) {
					Timer_Stop();
					LCD_ClrarScreen();
				}
				else if(result == 1 || (check_password() && !alarm_latched))
				{
					Timer_Stop();
						if(alarm_latched)
						{
							alarm_latched = 0;
							if(!vibration_latched) alarm_off();
						}
                        LCD_ClrarScreen();
                        LCD_SendString("Opened");
                        auth = 1;
                        PORTC |= (1 << PC0);
                        _delay_ms(500);
                        PORTC &= ~(1 << PC0);
						_delay_ms(500);
                        LCD_ClrarScreen();
                        tries = 0;
                    }
                    else
                    {
						Timer_Stop();
                        LCD_ClrarScreen();
                        LCD_SendString("wrong");
                        wrong_beep();
                        tries++;
                        _delay_ms(250);
                        LCD_ClrarScreen();
                    }
                }
                break;
//------------------------------Case C if you want change pass------------------------------
                case 'C':
                {
	                LCD_ClrarScreen();
	                LCD_MoveCursor(1,1);
	                LCD_SendString("Enter old pass:");
	                LCD_MoveCursor(2,1);
	                char old_result = get_input();
	                if(old_result == 3){
		                Timer_Stop();
		                timer();
	                }
	                else if(old_result == 2)
	                {
		                Timer_Stop();
		                LCD_ClrarScreen();
	                }
	                else if(old_result == 1 || check_password() || switch_ok())
	                {
		                Timer_Stop();
		                auth = 1;
		                LCD_ClrarScreen();
		                LCD_SendString("Correct");
		                _delay_ms(1000);
		                LCD_ClrarScreen();
		                while(switch_ok());
		                _delay_ms(50);
		                LCD_MoveCursor(1,1);
		                LCD_SendString("Enter new pass:");
		                LCD_MoveCursor(2,1);
		                char new_result = get_input();
		                if(new_result == 3){
			                Timer_Stop();
			                timer();
		                }
		                else if(new_result == 2)
		                {
			                Timer_Stop();
			                LCD_ClrarScreen();
		                }
		                else
		                {
			                Timer_Stop();
			                ChangePassword();
			                auth = 1;
			                tries = 0;
		                }
	                }
	                else  
	                {
		                Timer_Stop();
		                LCD_ClrarScreen();
		                LCD_SendString("wrong");
		                wrong_beep();
		                tries++;       
		                _delay_ms(250);
		                LCD_ClrarScreen();
	                }
                }
                break;
//------------------------------Case D if you want turn 0n or off transmission and breakage protection mode------------------------------				
				case 'D':
				{
					LCD_ClrarScreen();
					LCD_MoveCursor(1,1);
					LCD_SendString("Enter pass:");
					LCD_MoveCursor(2,1);
					char result = get_input();
					if(result == 3){
						Timer_Stop();
						timer();
					}
					else if(result == 2)
					{
						Timer_Stop();
						LCD_ClrarScreen();
					}
					else if(result == 1 || (check_password() && !alarm_latched))
					{
						Timer_Stop();
						auth = 1;
						if(vibration_ok == 0){
						
							LCD_ClrarScreen();
							LCD_MoveCursor(1,1);
							LCD_SendString("Transport protection");
							LCD_MoveCursor(2,1);
							LCD_SendString("activated");
							
							vibration_ok = 1;
							_delay_ms(1500);
							
							LCD_ClrarScreen();
							tries = 0;
						}else if(vibration_ok == 1){
							Timer_Stop();
							
							LCD_ClrarScreen();
							LCD_MoveCursor(1,1);
							LCD_SendString("Transport protection");
							LCD_MoveCursor(2,1);
							LCD_SendString("disabled");
							
							vibration_ok = 0;
							vibration_latched = 0;
							if(!alarm_latched) alarm_off();
							_delay_ms(1500);
							
							LCD_ClrarScreen();
							tries = 0;
						}
						
					}
					else
					{
						LCD_ClrarScreen();
						LCD_SendString("wrong");
						wrong_beep();
						tries++;
						_delay_ms(250);
						LCD_ClrarScreen();
					}
				}
				break;
            }
            if(tries >= 3)
            {
                alarm_latched = 1;
                tries = 0;
            }
        }
    }
}