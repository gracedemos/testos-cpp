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
size_t selected;
size_t selRow;
size_t oRow;

void UpdateCursor(size_t, size_t);
void WriteOptionSelect(const char*, size_t);
void WriteOptionLine(const char*, size_t, size_t);

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
	UpdateCursor(terminalColumn, terminalRow);
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
	UpdateCursor(terminalColumn, terminalRow);
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

void InterruptHandler() {
	status = InB(0x64);
	scancode = InB(0x60);

	switch(scancode) {
		case 0x1F: //S
			selected++;
			terminalRow = selRow;
			terminalColumn = 0;
			WriteOptionLine("Option 1", 0, 0);
			TerminalNextLine();
			WriteOptionLine("Option 2", 1, 1);
			TerminalNextLine();
			break;
		case 0x11:
			selected = selected - 1;
			terminalRow = selRow;
			terminalColumn = 0;
			WriteOptionLine("Option 1", 1, 0);
			TerminalNextLine();
			WriteOptionLine("Option 2", 0, 1);
			TerminalNextLine();
			break;
		case 0x1C:
			switch(selected) {
				case 0:
					terminalRow = oRow;
					terminalColumn = 0;
					terminalColor = VGAEntryColor(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
					TerminalWriteString("1");
					terminalColor = VGAEntryColor(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
					break;
				case 1:
					terminalRow = oRow;
					terminalColumn = 0;
					terminalColor = VGAEntryColor(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
					TerminalWriteString("2");
					terminalColor = VGAEntryColor(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
					break;
				}
			break;
	}
}

void Hang() {
	while(1) {

	}
}

void WaitForInterrupt() {
	size_t wait = 1;
	while(wait) {
		InterruptHandler();
		wait = 0;
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

extern "C" {
	void KernelMain() {
		TerminalInitialize();
		DisableCursor();
		TerminalSplash();

		selRow = terminalRow;
		WriteOptionLine("Option 1", 1, 0);
		TerminalNextLine();
		WriteOptionLine("Option 2", 0, 1);
		TerminalNextLine();
		TerminalNextLine();

		oRow = terminalRow;
		while(1) {
			WaitForInterrupt();
		}

		Hang();
	}
}
