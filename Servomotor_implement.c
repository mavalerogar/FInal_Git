#include "msp.h"
#include "lcdlib (1).h"
#define PIN_LENGTH 4

void setup(void);
char keypad(void);
void handleKeyPress(void);
void PORT3_IRQHandler(void);
void PORT5_IRQHandler(void);
void configurePWM(void);
void countDown(void);
void configureServoTimer(void);
void TA0_0_IRQHandler(void);
void configureBuzzerPWM(void);

volatile int timeIndex = 0;
volatile char enteredTime[PIN_LENGTH + 1] = {0}; // Buffer to store the time
volatile char lastKeyPress = '\0';
volatile int s = 0, m = 0; // Timer variables
volatile int timerRunning = 0; // Flag to indicate if the timer is running
volatile int servoDutyCycle = 1200; // Initial duty cycle for servo (1.5ms pulse width)
volatile int buzzerDutyCycle = 0; // Initial buzzer duty cycle

avoid main(void) {
    WDT_A->CTL = WDT_A_CTL_PW | WDT_A_CTL_HOLD; // Stop watchdog timer
    lcdInit();
    lcdClear();
    lcdSetText("Enter Time:", 0, 0);
    setup();
    configurePWM();
    configureServoTimer();
    configureBuzzerPWM();  // Configure buzzer PWM here
    __enable_irq(); // Enable global interrupts

    while (1) {
        if (timerRunning) {
            countDown();
            __delay_cycles(3000000); // Delay for 1 second (assuming 3 MHz clock)
        } else {
            __sleep();
            __no_operation();
        }
    }
}

void setup(void) {
    P5->DIR |= (BIT4 | BIT5 | BIT6 | BIT7); // Rows as output
    P5->OUT &= ~(BIT4 | BIT5 | BIT6 | BIT7); // Set rows low
    P3->DIR &= ~(BIT5 | BIT6 | BIT7); // Columns as input
    P3->REN |= (BIT5 | BIT6 | BIT7); // Enable pull-up/pull-down resistors
    P3->OUT |= (BIT5 | BIT6 | BIT7); // Pull-up resistors

    // Enable interrupts for P5.4, P5.5, P5.6, and P5.7
    P5->IE |= (BIT4 | BIT5 | BIT6 | BIT7); // Enable interrupt on P5.4, P5.5, P5.6, and P5.7
    P5->IES |= (BIT4 | BIT5 | BIT6 | BIT7); // Set interrupt to trigger on high-to-low transition
    P5->IFG &= ~(BIT4 | BIT5 | BIT6 | BIT7); // Clear interrupt flags
    NVIC->ISER[1] = 1 << ((PORT5_IRQn) & 31); // Enable Port 5 interrupt in NVIC

    // Enable interrupts for P3.5, P3.6, and P3.7
    P3->IE |= (BIT5 | BIT6 | BIT7); // Enable interrupt on P3.5, P3.6, and P3.7
    P3->IES |= (BIT5 | BIT6 | BIT7); // Set interrupt to trigger on high-to-low transition
    P3->IFG &= ~(BIT5 | BIT6 | BIT7); // Clear interrupt flags
    NVIC->ISER[1] = 1 << ((PORT3_IRQn) & 31); // Enable Port 3 interrupt in NVIC
}

void configurePWM() {
    P2->DIR |= BIT4; // Set P2.4 as output
    P2->SEL0 |= BIT4; // Select primary module function for P2.4
    P2->SEL1 &= ~BIT4; // Select primary module function for P2.4

    TIMER_A0->CCR[0] = 300000 - 1; // PWM Period (# of ticks in one period for 50Hz)
    TIMER_A0->CCTL[1] = TIMER_A_CCTLN_OUTMOD_7; // CCR1 reset/set mode
    TIMER_A0->CCR[1] = servoDutyCycle; // CCR1 PWM duty cycle (1.5ms pulse width for 0 degrees)
    TIMER_A0->CTL = TIMER_A_CTL_SSEL__SMCLK | TIMER_A_CTL_MC__UP | TIMER_A_CTL_CLR; // SMCLK, Up mode, clear TAR
}

void configureServoTimer() {
    TIMER_A1->CCR[0] = 30000 - 1; // Servo update period
    TIMER_A1->CCTL[0] = TIMER_A_CCTLN_CCIE; // Enable interrupt
    TIMER_A1->CTL = TIMER_A_CTL_SSEL__SMCLK | TIMER_A_CTL_MC__UP | TIMER_A_CTL_CLR; // SMCLK, Up mode, clear TAR

    NVIC->ISER[1] = 1 << ((TA1_0_IRQn) & 31); // Enable Timer_A1 interrupt in NVIC
}

void TA1_0_IRQHandler(void) {
    TIMER_A1->CCTL[0] &= ~TIMER_A_CCTLN_CCIFG; // Clear interrupt flag
    if (timerRunning) {
        TIMER_A0->CCR[1] = servoDutyCycle; // Update PWM duty cycle
    } else {
        TIMER_A0->CCR[1] = 0; // Stop PWM signal to servo
    }
}

char keypad(void) {
    const char keys[4][3] = { {'1', '2', '3'}, {'4', '5', '6'}, {'7', '8', '9'}, {'*', '0', '#'} };
    int row, col;
    for (row = 0; row < 4; row++) {
        P5->OUT |= (BIT4 | BIT5 | BIT6 | BIT7); // Set all rows high
        P5->OUT &= ~(BIT4 << row); // Set the current row low
        for (col = 0; col < 3; col++) {
            if (!(P3->IN & (BIT5 << col))) {
                __delay_cycles(30000); // Debouncing delay
                if (!(P3->IN & (BIT5 << col))) {
                    return keys[row][col];
                }
            }
        }
    }
    return '\0';
}

void PORT3_IRQHandler(void) {
    handleKeyPress();
    P3->IFG &= ~(BIT5 | BIT6 | BIT7); // Clear interrupt flags
}

void PORT5_IRQHandler(void) {
    handleKeyPress();
    P5->IFG &= ~(BIT4 | BIT5 | BIT6 | BIT7); // Clear interrupt flags
}

void handleKeyPress(void) {
    if (!timerRunning) {  // Only accept input if the timer is not running
        char keyPress = keypad();
        if (keyPress != '\0' && keyPress != lastKeyPress) {  // Check for a valid and new key press
            lastKeyPress = keyPress;
            if (keyPress >= '0' && keyPress <= '9' && timeIndex < PIN_LENGTH) {  // Check for digit and buffer overflow
                enteredTime[timeIndex++] = keyPress;  // Store the digit
                lcdSetText(enteredTime, 11, 0);  // Display time next to "Enter Time:"

                // Set buzzer duty cycle based on key press
                int volume = (keyPress - '0') * 100;  // Volume range: 0 to 900
                if (volume > 0) {
                    buzzerDutyCycle = volume;
                } else {
                    buzzerDutyCycle = 100;  // Set a small default duty cycle for sound
                }
                TIMER_A2->CCR[1] = buzzerDutyCycle;  // Update PWM duty cycle for buzzer
            }

            if (timeIndex == PIN_LENGTH) {  // When time length is reached
                enteredTime[PIN_LENGTH] = '\0';  // Null-terminate the string
                int totalSeconds = atoi(enteredTime);  // Convert entered time to total seconds
                int minutes = totalSeconds / 100;  // Extract minutes
                int seconds = totalSeconds % 100;  // Extract seconds
                if (seconds >= 60) {  // Convert to proper minute and second format
                    minutes += seconds / 60;
                    seconds = seconds % 60;
                }
                m = minutes;
                s = seconds;
                timerRunning = 1;  // Start the timer
                timeIndex = 0;  // Reset for next time entry
                lcdSetText("Enter Time:", 0, 0);  // Clear the entered time display
                delay_ms(100);
            }
        }
        __delay_cycles(600000);  // Prevent repeated key press detection (Debounce)
        lastKeyPress = '\0';  // Reset last key press after debounce delay
    }
}

void countDown() {
    int i;
    if (timerRunning) {
        if (s == 0 && m == 0) {
            timerRunning = 0; // Stop the timer when it reaches 00:00
            lcdClear();
            lcdSetText("Time's up!", 0, 0);
            TIMER_A0->CCR[1] = 0; // Stop PWM signal to servo
            buzzerDutyCycle = 500;  // Set buzzer duty cycle to generate sound
            TIMER_A2->CCR[1] = buzzerDutyCycle; // Update buzzer PWM to produce sound
            __delay_cycles(500000);  // Wait for a moment to let the buzzer beep
            buzzerDutyCycle = 0; // Stop buzzer
            TIMER_A2->CCR[1] = buzzerDutyCycle; // Update buzzer PWM
            delay_ms(5000); // Show "Time's Up!" message for 5 seconds
            lcdClear();
            lcdSetText("Enter Time:", 0, 0); // Revert to "Enter Time!"
            timeIndex = 0; // Reset timeIndex for new input
            lastKeyPress = '\0'; // Reset lastKeyPress for new input
            for (i = 0; i < PIN_LENGTH; i++) {
                enteredTime[i] = '\0'; // Clear the enteredTime buffer
            }
        } else {
            if (s == 0) {
                s = 59;
                m--;
            } else {
                s--;
            }
            char timeStr[6];
            sprintf(timeStr, "%02d:%02d", m, s);
            lcdSetText(timeStr, 11, 0); // Update time next to "Enter Time:"
            // Adjust duty cycle to spin the servo motor
            TIMER_A0->CCR[1] = servoDutyCycle; // 1.5ms pulse width for 0 degrees
            __delay_cycles(300000); // Increased delay to slow down the servo motor

            // Beep the buzzer
            if (s % 10 == 0) { // Buzzer beep every 10 seconds
                buzzerDutyCycle = 500; // Adjust the buzzer's volume
                TIMER_A2->CCR[1] = buzzerDutyCycle; // Update buzzer PWM
            }
        }
    }
}


void configureBuzzerPWM() {
    P2->DIR |= BIT5; // Set P2.5 as output
    P2->SEL0 |= BIT5; // Select primary module function for P2.5
    P2->SEL1 &= ~BIT5; // Select primary module function for P2.5

    TIMER_A2->CCR[0] = 1000 - 1; // PWM Period for buzzer (1 kHz)
    TIMER_A2->CCTL[1] = TIMER_A_CCTLN_OUTMOD_7; // CCR1 reset/set mode
    TIMER_A2->CCR[1] = buzzerDutyCycle; // CCR1 PWM duty cycle (initially 0)
    TIMER_A2->CTL = TIMER_A_CTL_SSEL__SMCLK | TIMER_A_CTL_MC__UP | TIMER_A_CTL_CLR; // SMCLK, Up mode, clear TAR
}
