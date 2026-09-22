#include <avr/io.h>
#define F_CPU 16000000UL
#include <util/delay.h>

// ==================== PINS ====================
// Motors: Right = IN1(PD3) IN2(PD4) | Left = IN3(PD5) IN4(PD6)
#define IN1 PD3
#define IN2 PD4
#define IN3 PD5
#define IN4 PD6
#define MOTORS_MASK ((1<<IN1)|(1<<IN2)|(1<<IN3)|(1<<IN4))

// Ultrasonic (Port B): front=PB0/PB1, right=PB2/PB3, back=PB4/PB5
#define FRONT_TRIG PB0
#define FRONT_ECHO PB1
#define RIGHT_TRIG PB2
#define RIGHT_ECHO PB3
#define BACK_TRIG  PB4
#define BACK_ECHO  PB5

// ==================== SETTINGS ====================
#define ENEMY_CM  30    // enemy detection range
#define NO_ECHO   999   // returned on timeout

// ==================== MOTORS ====================
static void motors_stop(void)  { PORTD &= ~MOTORS_MASK; }

static void motors_forward(void) {
    PORTD = (PORTD & ~MOTORS_MASK) | (1<<IN1) | (1<<IN3);
}
static void motors_spin_right(void) {  // left fwd, right back -> rotate right
    PORTD = (PORTD & ~MOTORS_MASK) | (1<<IN2) | (1<<IN3);
}

// ==================== SENSORS ====================
// Returns distance in cm, or NO_ECHO on timeout
static uint16_t ultrasonic_cm(uint8_t trig, uint8_t echo) {
    PORTB &= ~(1<<trig);
    _delay_us(2);
    PORTB |= (1<<trig);
    _delay_us(10);
    PORTB &= ~(1<<trig);

    uint16_t t = 0;
    while (!(PINB & (1<<echo))) {
        if (++t > 5000) return NO_ECHO;
        _delay_us(1);
    }

    uint16_t width = 0;
    while (PINB & (1<<echo)) {
        if (++width > 6000) return NO_ECHO;
        _delay_us(1);
    }
    return width / 58;   // us -> cm
}

static uint8_t enemy_front(void) { return ultrasonic_cm(FRONT_TRIG, FRONT_ECHO) < ENEMY_CM; }
static uint8_t enemy_right(void) { return ultrasonic_cm(RIGHT_TRIG, RIGHT_ECHO) < ENEMY_CM; }
static uint8_t enemy_back(void)  { return ultrasonic_cm(BACK_TRIG,  BACK_ECHO)  < ENEMY_CM; }

// ==================== STATES ====================
typedef enum {
    ST_SEARCH,
    ST_ATTACK
} state_t;

// ==================== INIT ====================
static void init(void) {
    DDRD  |= MOTORS_MASK;
    PORTD &= ~MOTORS_MASK;

    DDRB |=  (1<<FRONT_TRIG)|(1<<RIGHT_TRIG)|(1<<BACK_TRIG);
    DDRB &= ~((1<<FRONT_ECHO)|(1<<RIGHT_ECHO)|(1<<BACK_ECHO));
}

// ==================== MAIN ====================
int main(void) {
    init();

    motors_stop();
    _delay_ms(5000);          // rule 8.18: wait 5s before moving

    state_t state = ST_SEARCH;

    while (1) {
        switch (state) {

        // ---------- spin in place scanning front/right/back ----------
        case ST_SEARCH:
            if (enemy_front() || enemy_right() || enemy_back()) {
                state = ST_ATTACK;
                break;
            }
            motors_spin_right();
            _delay_ms(60);      // TUNE: rotation step per scan
            motors_stop();
            _delay_ms(20);      // let sonar settle before next reading
            break;

        // ---------- push forward while target stays in front ----------
        case ST_ATTACK:
            if (!enemy_front()) {
                state = ST_SEARCH;
                break;
            }
            motors_forward();
            break;
        }
    }
    return 0;
}
