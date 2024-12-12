#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define MAC

#ifdef MAC
#include <unistd.h>
#else
#include <Windows.h>
#include "rs232.h"
#include <conio.h>
#endif

#include "serial.h"

#define bdrate 115200        /* 115200 baud */
#define BUFFER_SIZE 256
#define LINE_SPACING 30.0     // 30mm
#define CHAR_WIDTH 6.0       // Default character width in font file units
#define FONT_SCALE 18.0     
// Global variables
float scaleFactor = 1.0;     // Scaling factor for font size
float xPosition = 0.0;       // Current x position
float yPosition = 0.0;       // Current y position

// Function prototypes
void readFontData(const char *fileName);
float getTextHeight(float userHeight);
void readTextFile(const char *fileName);
void generateGcode(char letter, float xOffset, float yOffset, FILE *outputFile);
void resetPenToOrigin(FILE *outputFile);
void SendCommands(char *buffer);

// Struct for font characters
typedef struct {
    int asciiCode;
    int numMovements;
    float *dx;
    float *dy;
    int *penState;
} FontCharacter;

FontCharacter fontData[128]; // Array to store font data for ASCII characters

int main() {
    char buffer[100];

    // Initialize serial communication
    if (CanRS232PortBeOpened() == -1) {
        printf("\nUnable to open the COM port (specified in serial.h)");
        exit(0);
    }

    // Wake up the robot
    printf("\nAbout to wake up the robot\n");
    sprintf(buffer, "\n");
    PrintBuffer(&buffer[0]);
    
    #ifdef MAC
        sleep(0.1);
    #else
        Sleep(100);
    #endif

    WaitForDollar();
    printf("\nThe robot is now ready to draw\n");

    // Initialize the robot to the starting position
    sprintf(buffer, "G1 X0 Y0 F1000\n");
    SendCommands(buffer);

    sprintf(buffer, "M3\n");
    SendCommands(buffer);

    // Initialize the robot to the starting position
    sprintf(buffer, "G1 X0 Y0 F1000\n");
    SendCommands(buffer);

    sprintf(buffer, "M3\n");
    SendCommands(buffer);
    sprintf(buffer, "S0\n");
    SendCommands(buffer);

    // Font data and text input
    char fontFile[] = "SingleStrokeFont.txt";
    char textFile[BUFFER_SIZE];

    float userHeight;

    printf("Enter text height (between 4mm and 10mm): ");
    scanf("%f", &userHeight);

    if (userHeight < 4.0 || userHeight > 10.0) {
        printf("Error: Height must be between 4mm and 10mm.\n");
        return 1;
    }

    scaleFactor = getTextHeight(userHeight);

    // Load font data
    printf("Loading font data...\n");
    readFontData(fontFile);

    // Get the text file name
    printf("Enter the name of the text file to draw:");
    scanf("%s", textFile);

    // Process the text file
    printf("Processing text file...\n");
    readTextFile(textFile);

    // Reset pen to origin at the end
    FILE *outputFile = fopen("gcode_output.txt", "a");
    if (outputFile) {
        resetPenToOrigin(outputFile);
        fclose(outputFile);
    }

    printf("Done.\n");

    CloseRS232Port();
    printf("COM port now closed\n");

    return 0;

}


// Read font data from file
void readFontData(const char *fileName) {
    FILE *file = fopen(fileName, "r");
    if (file == NULL) {
        perror("Error opening font file");
        exit(1);
    }

    char line[BUFFER_SIZE];
    while (fgets(line, BUFFER_SIZE, file)) {
        int asciiCode, numMovements;
        if (sscanf(line, "999 %d %d", &asciiCode, &numMovements) == 2) {
            FontCharacter *character = &fontData[asciiCode];
            character->asciiCode = asciiCode;
            character->numMovements = numMovements;
            character->dx = malloc(numMovements * sizeof(float));
            character->dy = malloc(numMovements * sizeof(float));
            character->penState = malloc(numMovements * sizeof(int));

            for (int i = 0; i < numMovements; i++) {
                if (fgets(line, BUFFER_SIZE, file)) {
                    sscanf(line, "%f %f %d", &character->dx[i], &character->dy[i], &character->penState[i]);
                }
            }
        }
    }
    fclose(file);
}

// Function to get text height
float getTextHeight(float userHeight) {
    return userHeight / FONT_SCALE;
}

// Read text file for drawing
void readTextFile(const char *fileName) {
    FILE *file = fopen(fileName, "r");
    if (file == NULL) {
        perror("Error opening text file");
        exit(1);
    }

    FILE *outputFile = fopen("gcode_output.txt", "a");
    if (outputFile == NULL) {
        perror("Error creating output file");
        fclose(file);
        exit(1);
    }

    char word[BUFFER_SIZE];
    float xOffset = xPosition;
    float yOffset = yPosition;

    while (fgets(word, BUFFER_SIZE, file)) {
        size_t len = strlen(word);
        if (word[len - 1] == '\n') {
            word[len - 1] = '\0';
        }

        for (size_t i = 0; i < strlen(word); i++) {
            generateGcode(word[i], xOffset, yOffset, outputFile);
            xOffset += scaleFactor * CHAR_WIDTH;
        }

        //Move to the next line
        xOffset = 0.0;
        yOffset -= scaleFactor * LINE_SPACING;
    }

    fclose(file);
    fclose(outputFile);
}

// Function to generate G-code for a specific character
void generateGcode(char letter, float xOffset, float yOffset, FILE *outputFile) {
    FontCharacter *character = &fontData[letter];
    char gcodeBuffer[BUFFER_SIZE];
    int penState = 0;

    for (int i = 0; i < character->numMovements; i++) {
        float scaledX = xOffset + character->dx[i] * scaleFactor;
        float scaledY = yOffset + character->dy[i] * scaleFactor;

        if (character->penState[i] != penState) {
            sprintf(gcodeBuffer, "%d\n", character->penState[i] ? 1 : 0);
            fputs(gcodeBuffer, outputFile);
            printf("%s", gcodeBuffer);
            penState = character->penState[i];
        }

        sprintf(gcodeBuffer, "G1 X%.2f Y%.2f\n", scaledX, scaledY);
        fputs(gcodeBuffer, outputFile);
        printf("%s", gcodeBuffer);
    }
}

// Function to reset the pen to the origin
void resetPenToOrigin(FILE *outputFile) {
    char gcodeBuffer[BUFFER_SIZE];
    sprintf(gcodeBuffer, "G0 X0 Y0\n");
    fputs(gcodeBuffer, outputFile);
    printf("%s", gcodeBuffer);
}

// Function to send commands to the robot
void SendCommands(char *buffer) {
    // Print buffer to the serial communication interface
    PrintBuffer(&buffer[0]);
    WaitForReply();

    #ifdef MAC
        sleep(0.1);
    #else
        Sleep(100);
    #endif
}
        
    



 