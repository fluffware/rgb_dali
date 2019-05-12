#include <avr/io.h>
#include <avr/interrupt.h>
#include <sys/clock.h>
#include <sys/etimer.h>

static volatile clock_time_t current_clock = 0;
static volatile unsigned long current_seconds = 0;
static unsigned int second_countdown = CLOCK_SECOND;

ISR(TIMER0_COMPA_vect, ISR_NOBLOCK)
{
   current_clock++;
   if(etimer_pending() 
      && (etimer_next_expiration_time() - current_clock) <= 0) {
    etimer_request_poll();
  }
  if (--second_countdown == 0) {
    current_seconds++;
    second_countdown = CLOCK_SECOND;
  }
}

void
clock_init(void)
{
  TCCR0A = _BV(WGM01); // CTC
  TCCR0B = _BV(CS02) | _BV(CS00); // F_CPU / 1024
  TCNT0 = 0;
  OCR0A = 125 - 1;
  TIFR0 = _BV(OCF0B) | _BV(OCF0A) | _BV(TOV0);
  TIMSK0 = _BV(OCIE0A);
}

clock_time_t
clock_time(void)
{
  TIMSK0 &= ~_BV(OCIE0A);
  clock_time_t c = current_clock;
  TIMSK0 |= _BV(OCIE0A);
  return c;
}

unsigned long
clock_seconds(void)
{
  TIMSK0 &= ~_BV(OCIE0A);
  clock_time_t s = current_seconds;
  TIMSK0 |= _BV(OCIE0A);
  return s;
}
