#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum VGAColor {
	VGA_COLOR_BLACK = 0,
	VGA_COLOR_BLUE = 1,
	VGA_COLOR_GREEN = 2,
	VGA_COLOR_CYAN = 3,
	VGA_COLOR_RED = 4,
	VGA_COLOR_MAGENTA = 5,
	VGA_COLOR_BROWN = 6,
	VGA_COLOR_LIGHT_GREY = 7,
	VGA_COLOR_DARK_GREY = 8,
	VGA_COLOR_LIGHT_BLUE = 9,
	VGA_COLOR_LIGHT_GREEN = 10,
	VGA_COLOR_LIGHT_CYAN = 11,
	VGA_COLOR_LIGHT_RED = 12,
	VGA_COLOR_LIGHT_MAGENTA = 13,
	VGA_COLOR_LIGHT_BROWN = 14,
	VGA_COLOR_WHITE = 15,
};

static inline uint8_t VGAEntryColor(enum VGAColor fg, enum VGAColor bg) 
{
	return fg | bg << 4;
}
 
static inline uint16_t VGAEntry(unsigned char uc, uint8_t color) 
{
	return (uint16_t) uc | (uint16_t) color << 8;
}

static inline void OutB(uint16_t port, uint8_t val) {
	asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

static inline uint8_t InB(uint16_t port) {
	uint8_t ret;
	asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
	return ret;
}

size_t StrLen(const char* str) {
	size_t len = 0;
	while(str[len]) {
		len++;
	}
	return len;
}

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

size_t terminalRow;
size_t terminalColumn;
uint8_t terminalColor;
uint16_t* terminalBuffer;
unsigned char status;
unsigned char scancode;
unsigned char scanUp;
size_t selected;
size_t selRow;
size_t dValue = 0;
size_t mainB = 1;
size_t termB = 1;
size_t echoColumn = 2;
size_t shiftDown = 0;
char termLine[VGA_WIDTH];

const char* op1 = "Terminal";
const char* op2 = "Exit";

void UpdateCursor(size_t, size_t);
void WriteOptionSelect(const char*, size_t);
void WriteOptionLine(const char*, size_t, size_t);
void MainSelection();
void Hang();

void TerminalInitialize() {
	terminalRow = 0;
	terminalColumn = 0;
	terminalColor = VGAEntryColor(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	terminalBuffer = (uint16_t*)0xB8000;
	for(size_t y = 0; y < VGA_HEIGHT; y++) {
		for(size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminalBuffer[index] = VGAEntry(' ', terminalColor);
		}
	}
}

void TerminalSetColor(uint8_t color) {
	terminalColor = color;
}

void TerminalEntry(char c, uint8_t color, size_t x, size_t y) {
	const size_t index = y * VGA_WIDTH + x;
	terminalBuffer[index] = VGAEntry(c, color);
}

void TerminalWriteChar(char c) {
	TerminalEntry(c, terminalColor, terminalColumn, terminalRow);
	if(terminalColumn++ == VGA_WIDTH) {
		terminalColumn = 0;
		if(terminalRow++ == VGA_HEIGHT) {
			terminalRow = 0;
		}
	}
	UpdateCursor(terminalColumn, terminalRow + 1);
}

void TerminalWriteString(const char* data) {
	size_t size = StrLen(data);
	for(size_t i = 0; i < size; i++) {
		TerminalWriteChar(data[i]);
	}	
}

void TerminalWriteStringInv(const char* data) {
	size_t size = StrLen(data);
	terminalColor = VGAEntryColor(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
	for(size_t i = 0; i < size; i++) {
		TerminalWriteChar(data[i]);
	}
	terminalColor = VGAEntryColor(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

void UpdateCursor(size_t x, size_t y) {
	uint16_t pos = y * VGA_WIDTH + x;
	OutB(0x3D4, 0x0F);
	OutB(0x3D5, (uint8_t)(pos& 0xFF));
	OutB(0x3D4, 0x0E);
	OutB(0x3D5, (uint8_t)((pos >> 8)) & 0xFF);
}

void TerminalNextLine() {
	terminalRow++;
	terminalColumn = 0;
	UpdateCursor(terminalColumn, terminalRow + 1);
}

void TerminalSplash() {
	terminalColor = VGAEntryColor(VGA_COLOR_MAGENTA, VGA_COLOR_BLACK);
	TerminalWriteString(" _____         _    ___  ____  ");
	TerminalNextLine();
	TerminalWriteString("|_   _|__  ___| |_ / _ \\/ ___| ");
	TerminalNextLine();
	TerminalWriteString("  | |/ _ \\/ __| __| | | \\___ \\ ");
	TerminalNextLine();
	TerminalWriteString("  | |  __/\\__ \\ |_| |_| |___) |");
	TerminalNextLine();
	TerminalWriteString("  |_|\\___||___/\\__|\\___/|____/");
	terminalColor = VGAEntryColor(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	TerminalNextLine();
	TerminalNextLine();
	TerminalWriteString("Version 1.0");
	TerminalNextLine();
	TerminalNextLine();
}

void DisableCursor() {
	OutB(0x3D4, 0x0A);
	OutB(0x3D5, 0x20);
}

void EnableCursor() {
	OutB(0x3D4, 0x0A);
	OutB(0x3D5, (InB(0x3D5) & 0xC0) | (terminalRow + 1));
	OutB(0x3D4, 0x0B);
	OutB(0x3D5, (InB(0x3D5) & 0xE0) | (terminalRow + 1));
}

void InterruptHandlerMain() {
	status = InB(0x64);
	scancode = InB(0x60);

	switch(scancode) {
		case 0x50: //Down Arrow
			selected++;
			terminalRow = selRow;
			terminalColumn = 0;
			WriteOptionLine(op1, 0, 0);
			TerminalNextLine();
			WriteOptionLine(op2, 1, 1);
			TerminalNextLine();
			break;
		case 0x48: //Up Arrow
			selected = selected - 1;
			terminalRow = selRow;
			terminalColumn = 0;
			WriteOptionLine(op1, 1, 0);
			TerminalNextLine();
			WriteOptionLine(op2, 0, 1);
			TerminalNextLine();
			break;
		case 0x1C: //Enter
			switch(selected) {
				case 0:
					mainB = 0;
					break;
				case 1:
					TerminalInitialize();
					DisableCursor();
					Hang();
					break;
				}
			break;
	}
}

void Hang() {
	while(1) {

	}
}

void WaitForInterruptMain() {
	scanUp = scancode;
	if(InB(0x60) == scanUp) {

	}
	else {
		InterruptHandlerMain();
	}
}

void WriteOptionLine(const char* data, size_t seld, size_t num) {
	if(seld) {
		TerminalWriteStringInv(data);
		selected = num;
	}
	else {
		TerminalWriteString(data);
	}
}

void TerminalEcho() {
	if(terminalRow == VGA_HEIGHT) {
		TerminalInitialize();
		EnableCursor();
	}
	terminalColor = VGAEntryColor(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
	TerminalWriteString("$ ");
	terminalColor = VGAEntryColor(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

size_t GetCursorPosition() {
	size_t pos = 0;
	OutB(0x3D4, 0x0F);
	pos |= InB(0x3D5);
	OutB(0x3D4, 0x0E);
	pos |= ((size_t)InB(0x3D5)) << 8;
	return pos;
}

void TerminalInterruptHandler() {
	status = InB(0x64);
	scancode = InB(0x60);

	switch(scancode) {
		case 0x8E: //Backspace
			if(terminalColumn == echoColumn) {

			}
			else {
				terminalColumn -= 1;
				TerminalWriteString(" ");
				terminalColumn -= 1;
				UpdateCursor(terminalColumn, terminalRow + 1);
				termLine[terminalColumn - 1] = ' ';
			}
			break;
		case 0x39: //Space
			termLine[terminalColumn - 2] = ' ';
			TerminalWriteString(" ");
			break;
		case 0x1C: //Enter
			TerminalNextLine();
			if(termLine[0] == '1' && termLine[1] == '3' && termLine[2] == '5' && termLine[3] == '8') {
				TerminalWriteString("Correct");
				TerminalNextLine();
			}
			if(termLine[0] == 'e' && termLine[1] == 'x' && termLine[2] == 'i' && termLine[3] == 't') {
				termB = 0;
			}
			if(termLine[0] == 'p' && termLine[1] == 'r' && termLine[2] == 'i' && termLine[3] == 'n' && termLine[4] == 't') {
				for(size_t i = 0; i < 75; i++) {
					if(i > 5) {
						TerminalWriteChar(termLine[i]);
					}
				}
				TerminalNextLine();
			}
			if(termLine[0] == 's' && termLine[1] == 'p' && termLine[2] == 'l' && termLine[3] == 'a' && termLine[4] == 's' && termLine[5] == 'h') {
				TerminalSplash();
			}
			if(termLine[0] == 'h' && termLine[1] == 'e' && termLine[2] == 'l' && termLine[3] == 'p') {
				TerminalWriteString("print: prints a string");
				TerminalNextLine();
				TerminalWriteString("exit: exit terminal");
				TerminalNextLine();
				TerminalWriteString("splash: prints TestOS splash");
				TerminalNextLine();
			}
			for(size_t i = 0; i < VGA_WIDTH; i++) {
				termLine[i] = ' ';
			}
			TerminalEcho();
			break;
		case 0x2A: //Left Shift
			shiftDown = 1;
			break;
		case 0x36: //Right Shift
			shiftDown = 1;
			break;
		case 0xAA: //Left Shift Up
			shiftDown = 0;
			break;
		case 0xB6: //Right Shift Up
			shiftDown = 0;
			break;
		case 0x02: //1
			termLine[terminalColumn - 2] = '1';
			TerminalWriteString("1");
			break;
		case 0x03: //2
			termLine[terminalColumn - 2] = '2';
			TerminalWriteString("2");
			break;
		case 0x04: //3
			termLine[terminalColumn - 2] = '3';
			TerminalWriteString("3");
			break;
		case 0x05: //4
			termLine[terminalColumn - 2] = '4';
			TerminalWriteString("4");
			break;
		case 0x06: //5
			termLine[terminalColumn - 2] = '5';
			TerminalWriteString("5");
			break;
		case 0x07: //6
			termLine[terminalColumn - 2] = '6';
			TerminalWriteString("6");
			break;
		case 0x08: //7
			termLine[terminalColumn - 2] = '7';
			TerminalWriteString("7");
			break;
		case 0x09: //8
			termLine[terminalColumn - 2] = '8';
			TerminalWriteString("8");
			break;
		case 0x0A: //9
			termLine[terminalColumn - 2] = '9';
			TerminalWriteString("9");
			break;
		case 0x0B: //0
			termLine[terminalColumn - 2] = '0';
			TerminalWriteString("0");
			break;
		case 0x12: //E
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'E';
				TerminalWriteString("E");
			}
			else {
				termLine[terminalColumn - 2] = 'e';
				TerminalWriteString("e");
			}
			break;
		case 0x2D: //X
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'X';
				TerminalWriteString("X");
			}
			else {
				termLine[terminalColumn - 2] = 'x';
				TerminalWriteString("x");
			}
			break;
		case 0x17: //I
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'I';
				TerminalWriteString("I");
			}
			else {
				termLine[terminalColumn - 2] = 'i';
				TerminalWriteString("i");
			}
			break;
		case 0x14: //T
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'T';
				TerminalWriteString("T");
			}
			else {
				termLine[terminalColumn - 2] = 't';
				TerminalWriteString("t");
			}
			break;
		case 0x1E: //A
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'A';
				TerminalWriteString("A");
			}
			else {
				termLine[terminalColumn - 2] = 'a';
				TerminalWriteString("a");
			}
			break;
		case 0x30: //B
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'B';
				TerminalWriteString("B");
			}
			else {
				termLine[terminalColumn - 2] = 'b';
				TerminalWriteString("b");
			}
			break;
		case 0x2E: //C
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'C';
				TerminalWriteString("C");
			}
			else {
				termLine[terminalColumn - 2] = 'c';
				TerminalWriteString("c");
			}
			break;
		case 0x20: //D
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'D';
				TerminalWriteString("D");
			}
			else {
				termLine[terminalColumn - 2] = 'd';
				TerminalWriteString("d");
			}
			break;
		case 0x21: //F
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'F';
				TerminalWriteString("F");
			}
			else {
				termLine[terminalColumn - 2] = 'f';
				TerminalWriteString("f");
			}
			break;
		case 0x22: //G
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'G';
				TerminalWriteString("G");
			}
			else {
				termLine[terminalColumn - 2] = 'g';
				TerminalWriteString("g");
			}
			break;
		case 0x23: //H
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'H';
				TerminalWriteString("H");
			}
			else {
				termLine[terminalColumn - 2] = 'h';
				TerminalWriteString("h");
			}
			break;
		case 0x24: //J
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'J';
				TerminalWriteString("J");
			}
			else {
				termLine[terminalColumn - 2] = 'j';
				TerminalWriteString("j");
			}
			break;
		case 0x25: //K
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'K';
				TerminalWriteString("K");
			}
			else {
				termLine[terminalColumn - 2] = 'k';
				TerminalWriteString("k");
			}
			break;
		case 0x26: //L
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'L';
				TerminalWriteString("L");
			}
			else {
				termLine[terminalColumn - 2] = 'l';
				TerminalWriteString("l");
			}
			break;
		case 0x32: //M
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'M';
				TerminalWriteString("M");
			}
			else {
				termLine[terminalColumn - 2] = 'm';
				TerminalWriteString("m");
			}
			break;
		case 0x31: //N
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'N';
				TerminalWriteString("N");
			}
			else {
				termLine[terminalColumn - 2] = 'n';
				TerminalWriteString("n");
			}
			break;
		case 0x18: //O
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'O';
				TerminalWriteString("O");
			}
			else {
				termLine[terminalColumn - 2] = 'o';
				TerminalWriteString("o");
			}
			break;
		case 0x19: //P
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'P';
				TerminalWriteString("P");
			}
			else {
				termLine[terminalColumn - 2] = 'p';
				TerminalWriteString("p");
			}
			break;
		case 0x10: //Q
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'Q';
				TerminalWriteString("Q");
			}
			else {
				termLine[terminalColumn - 2] = 'q';
				TerminalWriteString("q");
			}
			break;
		case 0x13: //R
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'R';
				TerminalWriteString("R");
			}
			else {
				termLine[terminalColumn - 2] = 'r';
				TerminalWriteString("r");
			}
			break;
		case 0x1F: //S
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'S';
				TerminalWriteString("S");
			}
			else {
				termLine[terminalColumn - 2] = 's';
				TerminalWriteString("s");
			}
			break;
		case 0x16: //U
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'U';
				TerminalWriteString("U");
			}
			else {
				termLine[terminalColumn - 2] = 'u';
				TerminalWriteString("u");
			}
			break;
		case 0x2F: //V
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'V';
				TerminalWriteString("V");
			}
			else {
				termLine[terminalColumn - 2] = 'v';
				TerminalWriteString("v");
			}
			break;
		case 0x11: //W
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'W';
				TerminalWriteString("W");
			}
			else {
				termLine[terminalColumn - 2] = 'w';
				TerminalWriteString("w");
			}
			break;
		case 0x15: //Y
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'Y';
				TerminalWriteString("Y");
			}
			else {
				termLine[terminalColumn - 2] = 'y';
				TerminalWriteString("y");
			}
			break;
		case 0x2C: //Z
			if(shiftDown) {
				termLine[terminalColumn - 2] = 'Z';
				TerminalWriteString("Z");
			}
			else {
				termLine[terminalColumn - 2] = 'z';
				TerminalWriteString("z");
			}
			break;
	}
}

void WaitForInterruptTerminal() {
	scanUp = scancode;
	if(InB(0x60) == scanUp) {

	}
	else {
		TerminalInterruptHandler();
	}
}

void TerminalMain() {
	TerminalInitialize();
	EnableCursor();
	TerminalEcho();

	while(termB) {
		WaitForInterruptTerminal();
	}

	termB = 1;
	mainB = 1;
}

void MainSelection() {
	selRow = terminalRow;
	WriteOptionLine(op1, 1, 0);
	TerminalNextLine();
	WriteOptionLine(op2, 0, 1);
	TerminalNextLine();
	TerminalNextLine();
}

void StartWait() {
	size_t sc = InB(0x60);
	if(sc == 0x1C) {
		while(InB(0x60) != 0x9C) {

		}
	}
}

extern "C" {
	void KernelMain() {
		while(1) {
			TerminalInitialize();
			DisableCursor();
			TerminalSplash();

			MainSelection();
			StartWait();

			while(mainB) {
				WaitForInterruptMain();
			}

			TerminalMain();
		}

		Hang();
	}
}
