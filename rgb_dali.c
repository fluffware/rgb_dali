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


#define RECV_BUFFER_LEN 32 // must be 2^n
/* Bit 31-24 contains reception type */
#define RECV_8 0x01000000	// Received a valid 8 bit frame
#define RECV_16 0x02000000	// Received a valid 16 bit frame
#define RECV_24 0x03000000	// Received a valid 24 bit frame
#define TIMING_VILOATION 0x81000000 // Timing violation
#define ENCODING_VILOATION 0x82000000 // Encoding violation
#define ILLEGAL_LENGTH 0x83000000 // Not a 16 or 24 bit frame


volatile uint32_t recv_buffer[RECV_BUFFER_LEN];
volatile uint8_t recv_buffer_write = 0;
volatile uint8_t recv_buffer_read = 0;

#define US_TO_TICK(us) ((us)/4)
#define MIN_HALF_BIT US_TO_TICK(333)
#define MAX_HALF_BIT US_TO_TICK(520)
#define MIN_FULL_BIT US_TO_TICK(640)
#define MAX_FULL_BIT US_TO_TICK(1020)
#define MIN_STOP_CONDITION US_TO_TICK(2300)
#define BACK_TO_FORW_SETTLING

static volatile uint8_t half_bit_count = 0;
static volatile uint32_t recv_word = 0;
static volatile struct  {
  uint8_t aborted: 1;
  uint8_t timing_violation: 1;
  uint8_t encoding_violation: 1;
} recv_flags = {0};
static volatile uint16_t prev_frame_interval;
  
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
  if (recv_flags.aborted) return;
#if 0
  {
    uint8_t next = (recv_buffer_write + 1) & (RECV_BUFFER_LEN - 1);
    if (next != recv_buffer_read) {
      recv_buffer[recv_buffer_write] = t;
      recv_buffer_write = next;
    }
  }
#endif
  if (half_bit_count == 0) { // Start flank
    prev_frame_interval = t;
    half_bit_count++;
    return;
  }
  if (t <= MAX_HALF_BIT) {
    if (t < MIN_HALF_BIT) goto timing_violation;
    half_bit_count++;
  } else {
    if (half_bit_count == 1) goto timing_violation;
    if (t < MIN_FULL_BIT) goto timing_violation;
    if (t > MAX_FULL_BIT) goto timing_violation;
    if (half_bit_count & 1) goto encoding_violation;
    half_bit_count += 2;
  }
    
  if ((half_bit_count & 1) == 0) { // Middle of bit
    if (!(PIND & _BV(PD2))) {
      recv_word = (recv_word << 1) | 1;
    } else {
      recv_word = recv_word << 1;
    }
  }
  
  
  return;
 timing_violation:
  recv_flags.timing_violation = 1;
  recv_flags.aborted = 1;
  return;
 encoding_violation:
  recv_flags.encoding_violation = 1;
  recv_flags.aborted = 1;
}


ISR(TIMER1_COMPB_vect)
{
  uint8_t next = (recv_buffer_write + 1) & (RECV_BUFFER_LEN - 1);
  uint32_t res;
  if (recv_flags.timing_violation) {
    res = TIMING_VILOATION | half_bit_count;
  } else if (recv_flags.encoding_violation) {
    res = ENCODING_VILOATION | half_bit_count;
  } else if (half_bit_count == 0) {
    return;
    // Frames that end with a zero is one halfbit longer */
  } else if (half_bit_count == (34 + ((~recv_word) & 1))) {
    res = RECV_16 | (recv_word & 0xffff);
  } else if (half_bit_count == (18 + ((~recv_word) & 1))) {
    res = RECV_8 | (recv_word & 0xff);
  } else if (half_bit_count == (50 + ((~recv_word) & 1))) {
    res = RECV_24 | (recv_word & 0xffffff);
  } else {
    res = ILLEGAL_LENGTH | half_bit_count;
 
  }
  if (next != recv_buffer_read) {
    recv_buffer[recv_buffer_write] = res;
    recv_buffer_write = next;
  }
  recv_flags.timing_violation = 0;
  recv_flags.encoding_violation = 0;
  recv_flags.aborted = 0;
  half_bit_count = 0;
  recv_word = 0;
}

static void
start_recv(void) {
  TIMSK1 = 0;
  // Clear counter
  TCNT1H = 0;
  TCNT1L = 0;
  TCCR1B =  _BV(CS11)|_BV(CS10); // F_CPU/64
  TIFR1 = _BV(OCF1A) | _BV(OCR1B);
  EIFR = _BV(INTF0);
  EIMSK |= _BV(INT0);
  TIMSK1 = _BV(OCIE1B);
}

static void
init_dali(void)
{
  EIMSK &= ~_BV(INT0);
  
  TIMSK1 = 0;
  TCCR1A = 0;
  TCCR1B =  _BV(CS11)|_BV(CS10); // F_CPU/64
  //TCCR1B =  _BV(CS11); // F_CPU/8
  //TCCR1B =  _BV(CS21); // F_CPU/256
  TIMSK1 = 0;
  // TX
  DDRC |= _BV(PC3);
  PORTC &= ~_BV(PC3);
  // RX
  DDRD &= ~PD2;
  PORTD |= PD2;

  // INT0 interrupt (on PD2)
  EICRA = (EICRA & ~_BV(ISC01)) | _BV(ISC00); // Interrupt on any change
  EIFR = _BV(INTF0);

  // Clear counter
  TCNT1H = 0;
  TCNT1L = 0;
  
  OCR1B = US_TO_TICK(2200); //Stop condition
  TIFR1 = _BV(OCF1A) | _BV(OCR1B);
  EIMSK |= _BV(INT0);
  TIMSK1 = _BV(OCIE1B);
}


#define HALF_BIT_TIME US_TO_TICK(416)
#define FULL_BIT_TIME US_TO_TICK(833)

volatile static uint32_t send_word=0;
volatile static uint8_t send_done = 0;

ISR(TIMER1_COMPA_vect)
{
  if (!send_done) {
    PORTC ^= _BV(PC3);
    if (!(PORTC & _BV(PC3))) {
      if (send_word == 0x80000000) {
	goto end_frame;  // Returned to high after zero
      }
      /* Rising edge */
      if (send_word & 0x80000000) { // Mid one
	send_word <<= 1;
	if (send_word & 0x80000000) {
	  if (send_word == 0x80000000) {
	    goto end_frame; // Already at high level, stop immediately
	  }
	  /* Need to shift level before one*/
	  OCR1A = HALF_BIT_TIME;
	} else {
	  /* Already at correct level for zero */
	  OCR1A = FULL_BIT_TIME;
	}
      } else {
	/* Preparing for one */
	OCR1A = HALF_BIT_TIME;
      }
    } else {
      /* Falling edge */
      if (!(send_word & 0x80000000)) { // Mid zero
	send_word <<= 1;
	if (send_word & 0x80000000) {
	  /* Already at correct level for one */
	  OCR1A = FULL_BIT_TIME;
	} else {
	  /* Need to shift level before zero */
	  OCR1A = HALF_BIT_TIME;
	}
      } else {
	/* Preparing for one */
	OCR1A = HALF_BIT_TIME;
      }
    }
    return;
  end_frame:
    send_done = 1;
    OCR1A = FULL_BIT_TIME; // Delay start of reception
  } else {
    start_recv();
  }
}

static void
dali_send8(uint8_t frame)
{
  send_word = (((uint32_t)frame) << 24) | 0x10400000;
  send_done = 0;
  printf("Sent: %08lx\n",send_word);
  EIMSK &= ~_BV(INT0);
  TIMSK1 = 0;
  TCCR1A = 0;
  TCCR1B =  _BV(WGM12) | _BV(CS11)|_BV(CS10); // F_CPU/64, CTC
  TIFR1 = _BV(OCF1A);
  TCNT1 = 0;
  OCR1A = HALF_BIT_TIME;
  TIMSK1 = _BV(OCIE1A);
}

int
main()
{
  stdout = &uart_stdout;
  DDRB|=0x20;
  //PORTB &= ~0x20;
  init_ser();
  init_dali();
  PORTB |= 0x20;
  sei();
  printf("Started\n");
  while(1) {
    if (recv_buffer_read != recv_buffer_write) {
      uint32_t frame = recv_buffer[recv_buffer_read];
      recv_buffer_read = ((recv_buffer_read + 1) 
			  & (RECV_BUFFER_LEN - 1));
      if (frame == (RECV_16 | 0x7191)) {
	dali_send8(0xff);
      }
      printf("I: %lu\n",prev_frame_interval*4L);
      printf("R: %08lx\n",frame);
    } else {
      //printf("T: %08lx\n", send_word);
    }
  }
  return 0;
}
