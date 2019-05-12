#include <stdio.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <sys/clock.h>
#include <sys/process.h>
#include <sys/etimer.h>
#include <dali.h>

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


void
send_ws2812(uint8_t *grb, uint16_t bytes)
{
  cli();
  asm volatile ("movw r18, %1\n"
		"call send_ws2812_s\n"
		:
		: "x" (grb), "r" (bytes)
		: "r18", "r19"
		);
  sei();
}

#define xstr(a) str(a)
#define str(a) #a

/*
  0 <= h < 252
  0 <= s < 256
  0 <= v < 256
*/
static void
hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
  uint16_t c = v * s; /* 0 <= c <= 65025 */
  uint16_t x = (c * (int32_t)(42 - abs((h % 84) - 42))) / 42;/* 0<= x <=65025 */
  uint16_t m = v*255 - c; /* 0 <= m <= 65025 */
  uint16_t rp,gp, bp;
  if (h < 84) {
    if (h < 42) {
      rp = c;
      gp = x;
    } else {
      rp = x;
      gp = c;
    }
    bp = 0;
  } else if (h < 168) {
     if (h < 126) {
      gp = c;
      bp = x;
    } else {
      gp = x;
      bp = c;
    }
     rp = 0;
  } else {
    if (h < 210) {
      rp = x;
      bp = c;
    } else {
      rp = c;
      bp = x;
    }
    gp = 0;
  }
  *r = (rp + m) / 255;
  *g = (gp + m) / 255;
  *b = (bp + m) / 255;
}
    
static uint8_t grb_values[] = {0xff, 0x00, 0x00, 0x00, 0x00, 0xff};
    
static uint8_t hsv_values[] = {0,255,255, 42,128,255};

PROCESS(tick_process,"Ticker");

PROCESS_THREAD(tick_process, ev, data)
{
  static struct etimer timer1;
  PROCESS_BEGIN();
  DDRD |= _BV(PD5);
  PORTD |= _BV(PD5);
  printf("Tick started\n");
  etimer_set(&timer1, CLOCK_SECOND/20);
  while(1) {
    PROCESS_WAIT_EVENT();
    if (etimer_expired(&timer1)) {
      for (int i = 0; i < 2; i++) {
      hsv_values[i*3] += 1;
      if (hsv_values[i*3] >= 252) hsv_values[i*3] -= 252;
      hsv_to_rgb(hsv_values[i*3], hsv_values[i*3+1], hsv_values[i*3+2],
		 &grb_values[i*3+1],  &grb_values[i*3],  &grb_values[i*3+2]); 
      }
      send_ws2812(grb_values, 6);
      etimer_reset(&timer1);
    }
  }
  PROCESS_END();
}

int
main()
{
  clock_init();
  process_init();
  stdout = &uart_stdout;
  init_ser();
 
  process_start(&etimer_process, NULL);
  process_start(&tick_process, NULL);
  process_start(&dali_process, NULL);
  sei();
  printf("Started\n");

  while(1) {
   
    process_run();
    
  }
  return 0;
}
