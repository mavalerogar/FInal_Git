#include "msp.h"
#include "lcdlib (1).h"

#define PIN_LENGTH 4
#define BUZZER_PIN BIT5  // Define the buzzer pin (P2.5)

void setup(void);
char keypad(void);
void startCountdown(int minutes, int seconds);
void handleKeyPress(void);
void PORT3_IRQHandler(void);
void PORT5_IRQHandler(void);

volatile int timeIndex = 0;
volatile char enteredTime[PIN_LENGTH + 1] = {0};  // Buffer to store the time
volatile char lastKeyPress = '\0';

void main(void) {
    WDT_A->CTL = WDT_A_CTL_PW | WDT_A_CTL_HOLD;  // Stop watchdog timer

    lcdInit();
    lcdClear();
    lcdSetText("Enter Time:", 0, 0);

    setup();
    __enable_irq();  // Enable global interrupts

    while (1) {
        __sleep();
        __no_operation();
    }
}

void setup(void) {
    P2->DIR |= BUZZER_PIN;  // Set P2.5 as output for the buzzer
    P2->OUT &= ~BUZZER_PIN; // Ensure the buzzer is off initially

    P5->DIR |= (BIT4 | BIT5 | BIT6 | BIT7);  // Rows as output
    P5->OUT &= ~(BIT4 | BIT5 | BIT6 | BIT7); // Set rows low

    P3->DIR &= ~(BIT5 | BIT6 | BIT7);        // Columns as input
    P3->REN |= (BIT5 | BIT6 | BIT7);         // Enable pull-up/pull-down resistors
    P3->OUT |= (BIT5 | BIT6 | BIT7);         // Pull-up resistors

    // Enable interrupts for P5.4, P5.5, P5.6, and P5.7
    P5->IE |= (BIT4 | BIT5 | BIT6 | BIT7);          // Enable interrupt on P5.4, P5.5, P5.6, and P5.7
    P5->IES |= (BIT4 | BIT5 | BIT6 | BIT7);         // Set interrupt to trigger on high-to-low transition
    P5->IFG &= ~(BIT4 | BIT5 | BIT6 | BIT7);        // Clear interrupt flags
    NVIC->ISER[1] = 1 << ((PORT5_IRQn) & 31); // Enable Port 5 interrupt in NVIC

    // Enable interrupts for P3.5, P3.6, and P3.7
    P3->IE |= (BIT5 | BIT6 | BIT7);          // Enable interrupt on P3.5, P3.6, and P3.7
    P3->IES |= (BIT5 | BIT6 | BIT7);         // Set interrupt to trigger on high-to-low transition
    P3->IFG &= ~(BIT5 | BIT6 | BIT7);        // Clear interrupt flags
    NVIC->ISER[1] = 1 << ((PORT3_IRQn) & 31); // Enable Port 3 interrupt in NVIC
}

char keypad(void) {
    const char keys[4][3] = {
        {'1', '2', '3'},
        {'4', '5', '6'},
        {'7', '8', '9'},
        {'*', '0', '#'}
    };
    int row, col;
    for (row = 0; row < 4; row++) {
        P5->OUT |= (BIT4 | BIT5 | BIT6 | BIT7);  // Set all rows high
        P5->OUT &= ~(BIT4 << row);  // Set the current row low
        for (col = 0; col < 3; col++) {
            if (!(P3->IN & (BIT5 << col))) {
                __delay_cycles(30000);  // Debouncing delay
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
    P3->IFG &= ~(BIT5 | BIT6 | BIT7);  // Clear interrupt flags
}

void PORT5_IRQHandler(void) {
    handleKeyPress();
    P5->IFG &= ~(BIT4 | BIT5 | BIT6 | BIT7);  // Clear interrupt flags
}

void handleKeyPress(void) {
    char keyPress = keypad();
    if (keyPress != '\0' && keyPress != lastKeyPress) {  // Check for a valid and new key press
        lastKeyPress = keyPress;
        if (keyPress >= '0' && keyPress <= '9' && timeIndex < PIN_LENGTH) {  // Check for digit and buffer overflow
            enteredTime[timeIndex++] = keyPress;  // Store the digit
            lcdSetText(enteredTime, 11, 0);

            // Sound the buzzer for 1 second
            P2->OUT |= BUZZER_PIN;  // Turn on the buzzer
            __delay_cycles(300000);  // Delay for 0.1 second (assuming 3MHz clock)
            P2->OUT &= ~BUZZER_PIN;  // Turn off the buzzer
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
            startCountdown(minutes, seconds);  // Start the countdown
            timeIndex = 0;  // Reset for next time entry
            lcdSetText("Enter Time:", 0, 0);
            delay_ms(100);
        }
    }
    __delay_cycles(600000);  // Prevent repeated key press detection (Debounce)
    lastKeyPress = '\0';     // Reset last key press after debounce delay
}

void startCountdown(int minutes, int seconds) {
    int totalSeconds = minutes * 60 + seconds;
    while (totalSeconds > 0) {
        int displayMinutes = totalSeconds / 60;
        int displaySeconds = totalSeconds % 60;
        lcdClear();
        lcdSetText("Time Left:", 0, 0);
        lcdSetText(":", 13, 0);
        lcdSetInt(displayMinutes, 12, 0);  // Display minutes at the first position
        lcdSetInt(displaySeconds, 14, 0);  // Display seconds at the next two positions
        delay_ms(3000);  // Wait for 1 second (adjusted delay)
        totalSeconds--;
    }
    lcdClear();
    lcdSetText("Time's Up!", 0, 0);

    // Make 4 short beeps
    int i;
    for (i = 0; i < 4; i++) {
        P2->OUT |= BUZZER_PIN;  // Turn on the buzzer
        __delay_cycles(1500000);  // Delay for 0.1 second (assuming 3MHz clock)
        P2->OUT &= ~BUZZER_PIN;  // Turn off the buzzer
        __delay_cycles(1500000);  // Delay for 0.1 second (assuming 3MHz clock)
    }

    lcdClear();
    lcdSetText("Enter Time:", 0, 0);
}

