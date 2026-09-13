#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define FS 25000
#define F_CPU 8000000UL
#define F_SCL 400000UL
#define OCR1A_VALUE ((F_CPU / FS) - 1)
#define QT2120_ADDR 0x1C

const uint8_t halfsteps[8] = {48, 42, 38, 36, 32, 28, 25, 24};
uint8_t noteCounter[8] = {0};
uint8_t noteState = 0, noteEnable = 0;
uint8_t out;
uint8_t keysPressed = 0;

void TWI_init()
{
    TWSR = 0x00;
    TWBR = ((F_CPU / F_SCL) - 16) / 2; // set I2C frequency to 100kHz
    TWCR = (1 << TWEN); //activate I2C interface
}

void TWI_start()
{
    //TWINT = 1 to clear it
    //TWSTA = 1 to set ATmega as master
    //TWEN = 1 to activate I2C interface
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);

    //I2C takes time to be enabled, so use while loop to wait til it's ready
    //before doing anything else
    while (!(TWCR & (1 << TWINT))); 
}

void TWI_stop()
{
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

void TWI_write(uint8_t data)
{
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}

uint8_t TWI_read_nack()
{
    TWCR = (1 << TWINT) | (1 << TWEN);
    while(!(TWCR & (1 << TWINT)));
    return TWDR;
}

uint8_t QT2120_read(uint8_t reg)
{

    TWI_start();
    TWI_write((QT2120_ADDR << 1) | 0);
    TWI_write(reg);

    TWI_start();
    TWI_write((QT2120_ADDR << 1) | 1 );
    uint8_t data = TWI_read_nack();
    TWI_stop();
    return data;
}

void QT2120_write(uint8_t reg, uint8_t data)
{

    TWI_start();
    TWI_write((QT2120_ADDR << 1) | 0);
    TWI_write(reg);
    TWI_write(data);
    TWI_stop();
}

ISR(TIMER1_COMPA_vect)
{
  for(uint8_t i = 0; i < 8; i++)
  {
    noteCounter[i]++;
    if(noteCounter[i] > halfsteps[i]) 
    {
      noteState ^= (1 << i);
      noteCounter[i] = 0;
    }
  }

  out = noteEnable & noteState;
  PORTD = (PORTD & 0x03) | (out << 2);
  PORTB = (PORTB & 0b00111111) | (out & 0xC0);
  
}

void IO_setup()
{

  DDRC &= ~(1 << PC2);       // CHANGE pin input
  PORTC |= (1 << PC2);       // enable pullup on CHANGE pin

  //OUTPUTS to speaker
  DDRD |= (1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7);
  DDRB |= (1 << PB6) | (1 << PB7);

}

void Timer1_setup()
{
    TCNT1 = 0;
    TCCR1A = 0;
    TCCR1B = 0;
    OCR1A = OCR1A_VALUE;
    TCCR1B |= (1 << WGM12);
    TCCR1B |= (1 << CS10);
    TIMSK1 |= (1 << OCIE1A);
    sei();
}

int main()
{
    IO_setup();
    TWI_init();
    _delay_ms(200);

    noteEnable = 0;
    Timer1_setup();

    while (1)
    {
      keysPressed = QT2120_read(0x03);
      noteEnable = keysPressed;
    }
}