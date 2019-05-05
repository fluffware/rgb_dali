#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>

static int uart_putchar(char c, FILE *stream);

static FILE uart_stdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
					    _FDEV_SETUP_WRITE);

static int
uart_putchar(char c, FILE *stream)
{
  loop_until_bit_is_set(UCSR0A, UDRE0);
  UDR0 = c;
  return 0;
}

#define SER_BPS 38400

static void
init_ser(void)
{
  UBRR0 = (F_CPU/16/SER_BPS - 1);

  UCSR0B = (1<<TXEN0);
  /* Set frame format: 8data, 1stop bit */
  UCSR0C = (3<<UCSZ00);
}

#define RECV_INT_BUFFER_LEN 8 // must be 2^n
volatile uint16_t recv_int_buffer[RECV_INT_BUFFER_LEN];
volatile uint8_t recv_int_buffer_write = 0;
volatile uint8_t recv_int_buffer_read = 0;

ISR(INT0_vect)
{
  uint8_t f;
  uint16_t t = TCNT1L;
  t |= TCNT1H<<8;
  f = TIFR1;
  TCNT1H = 0;
  TCNT1L = 0;
  TIFR1 = _BV(TOV1);
  if (f & _BV(TOV1)) {
    t = 0xffff;
  }
  // Add interval to buffer if there's room
  uint8_t next = (recv_int_buffer_write + 1) & (RECV_INT_BUFFER_LEN - 1);
  if (next != recv_int_buffer_read) {
    recv_int_buffer[recv_int_buffer_write] = t;
    recv_int_buffer_write = next;
  }
}

static void
init_dali_recv(void)
{
  
  TCCR1A = 0;
  TCCR1B = _BV(CS11); // F_CPU/8
  TIMSK1 = 0;
  
  EICRA = (EICRA & ~_BV(ISC01)) | _BV(ISC00); // Interrupt on any change
  EIMSK |= _BV(INT0);

  // Clear counter
  TCNT1H = 0;
  TCNT1L = 0;
  
}

int
main()
{
  stdout = &uart_stdout;
  DDRB|=0x20;
  //PORTB &= ~0x20;
  init_ser();
  init_dali_recv();
  PORTB |= 0x20;
  sei();
  printf("Started\n");
  while(1) {
    if (recv_int_buffer_read != recv_int_buffer_write) {
      printf("Int: %u\n",recv_int_buffer[recv_int_buffer_read]);
      recv_int_buffer_read = ((recv_int_buffer_read + 1) 
			      & (RECV_INT_BUFFER_LEN - 1));
    } else {
      //printf("T: %d\n", TCNT1);
    }
  }
  return 0;
}
