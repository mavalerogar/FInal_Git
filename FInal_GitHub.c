#include "msp.h"
#include "lcdlib (1).h"

#define PIN_LENGTH 3

void setup(void);
char keypad(void);
void startCountdown(int minutes, int seconds);

void main(void) {
    WDT_A->CTL = WDT_A_CTL_PW | WDT_A_CTL_HOLD;  // Stop watchdog timer

    lcdInit();
    lcdClear();
    lcdSetText("Enter Time:", 0, 0);

    setup();
    char enteredTime[PIN_LENGTH + 1] = {0};  // Buffer to store the time
    int timeIndex = 0;
    char keyPress = '\0';
    char lastKeyPress = '\0';

    while (1) {
        keyPress = keypad();
        if (keyPress != '\0' && keyPress != lastKeyPress) {
            if (keyPress >= '0' && keyPress <= '9' && timeIndex < PIN_LENGTH) {  // Check for digit and buffer overflow
                enteredTime[timeIndex++] = keyPress;  // Store the digit
                lcdSetText(enteredTime, 11, 0);
            }
            lastKeyPress = keyPress;

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
    }
}

void setup(void) {
    P5->DIR |= (BIT4 | BIT5 | BIT6 | BIT7);  // Rows as output
    P3->DIR &= ~(BIT5 | BIT6 | BIT7);        // Columns as input
    P3->REN |= (BIT5 | BIT6 | BIT7);         // Enable pull-up/pull-down
    P3->OUT |= (BIT5 | BIT6 | BIT7);         // Pull-up enabled
}

char keypad(void) {
    const char keys[4][3] = {
        {'1', '2', '3'},
        {'4', '5', '6'},
        {'7', '8', '9'},
        {'*', '0', '#'}
    };
    int row, col;
    for (row = 0; 4 > row; row++) {
        P5->OUT |= (BIT4 | BIT5 | BIT6 | BIT7);
        P5->OUT &= ~((1 << (row + 4)) & (BIT4 | BIT5 | BIT6 | BIT7));
        for (col = 0; 3 > col; col++) {
            if (!(P3->IN & (BIT5 << col))) {
                __delay_cycles(100000);  // Debouncing delay
                if (!(P3->IN & (BIT5 << col))) {
                    return keys[row][col];
                }
            }
        }
    }
    return '\0';
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
        delay_ms(3000);  // Wait for 1 second
        totalSeconds--;
    }
    lcdClear();
    lcdSetText("Time's Up!", 0, 0);
    delay_ms(5000);  // Show "Time's Up!" message for 5 seconds
    lcdClear();
    lcdSetText("Enter Time:", 0, 0);
}
