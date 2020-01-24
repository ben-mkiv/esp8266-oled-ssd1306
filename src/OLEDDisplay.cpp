/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 by ThingPulse, Daniel Eichhorn
 * Copyright (c) 2018 by Fabrice Weinberg
 * Copyright (c) 2019 by Helmut Tschemernjak - www.radioshuttle.de
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * ThingPulse invests considerable time and money to develop these open source libraries.
 * Please support us by buying our products (and not the clones) from
 * https://thingpulse.com
 *
 */

/*
 * TODO Helmut
 * - test/finish dislplay.printf() on mbed-os
 * - Finish _putc with drawLogBuffer when running display
 */

#include "OLEDDisplay.h"

#ifndef MIN
#define MIN(a, b) ({ __typeof__ (a) _a = (a);  __typeof__ (b) _b = (b);  _a < _b ? _a : _b; })
#endif //MIN

OLEDDisplay::OLEDDisplay() {

    displayWidth = 128;
    displayHeight = 64;
    displayBufferSize = displayWidth * displayHeight / 8;
    color = WHITE;
    geometry = GEOMETRY_128_64;
    textAlignment = TEXT_ALIGN_LEFT;
    fontData = ArialMT_Plain_10;
    fontTableLookupFunction = DefaultFontTableLookup;
    buffer = NULL;
#ifdef OLEDDISPLAY_DOUBLE_BUFFER
    buffer_back = NULL;
#endif
}

OLEDDisplay::~OLEDDisplay() {
    end();
}

bool OLEDDisplay::allocateBuffer() {

    logBufferSize = 0;
    logBufferFilled = 0;
    logBufferLine = 0;
    logBufferMaxLines = 0;
    logBuffer = NULL;

    if (!this->connect()) {
        DEBUG_OLEDDISPLAY("[OLEDDISPLAY][init] Can't establish connection to display\n");
        return false;
    }

    if (this->buffer == NULL) {
        this->buffer = (uint8_t *) malloc((sizeof(uint8_t) * displayBufferSize) + getBufferOffset());
        this->buffer += getBufferOffset();

        if (!this->buffer) {
            DEBUG_OLEDDISPLAY("[OLEDDISPLAY][init] Not enough memory to create display\n");
            return false;
        }
    }

#ifdef OLEDDISPLAY_DOUBLE_BUFFER
    if (this->buffer_back == NULL) {
        this->buffer_back = (uint8_t *) malloc((sizeof(uint8_t) * displayBufferSize) + getBufferOffset());
        this->buffer_back += getBufferOffset();

        if (!this->buffer_back) {
            DEBUG_OLEDDISPLAY("[OLEDDISPLAY][init] Not enough memory to create back buffer\n");
            free(this->buffer - getBufferOffset());
            return false;
        }
    }
#endif

    return true;
}

bool OLEDDisplay::init() {

    if (!allocateBuffer()) {
        return false;
    }

    sendInitCommands();
    resetDisplay();

    return true;
}

void OLEDDisplay::end() {
    if (this->buffer) {
        free(this->buffer - getBufferOffset());
        this->buffer = NULL;
    }
#ifdef OLEDDISPLAY_DOUBLE_BUFFER
    if (this->buffer_back) {
        free(this->buffer_back - getBufferOffset());
        this->buffer_back = NULL;
    }
#endif
    if (this->logBuffer != NULL) {
        free(this->logBuffer);
        this->logBuffer = NULL;
    }
}

void OLEDDisplay::resetDisplay(void) {
    clear();
#ifdef OLEDDISPLAY_DOUBLE_BUFFER
    memset(buffer_back, 1, displayBufferSize);
#endif
    display();
}

void OLEDDisplay::setColor(OLEDDISPLAY_COLOR color) {
    this->color = color;
}

OLEDDISPLAY_COLOR OLEDDisplay::getColor() {
    return this->color;
}

void OLEDDisplay::setPixel(int16_t x, int16_t y) {
    if (x >= 0 && x < this->width() && y >= 0 && y < this->height()) {
        switch (color) {
            case WHITE:
                buffer[x + (y / 8) * this->width()] |= (1 << (y & 7));
                break;
            case BLACK:
                buffer[x + (y / 8) * this->width()] &= ~(1 << (y & 7));
                break;
            case INVERSE:
                buffer[x + (y / 8) * this->width()] ^= (1 << (y & 7));
                break;
        }
    }
}

void OLEDDisplay::setPixelColor(int16_t x, int16_t y, OLEDDISPLAY_COLOR color) {
    if (x >= 0 && x < this->width() && y >= 0 && y < this->height()) {
        switch (color) {
            case WHITE:
                buffer[x + (y / 8) * this->width()] |= (1 << (y & 7));
                break;
            case BLACK:
                buffer[x + (y / 8) * this->width()] &= ~(1 << (y & 7));
                break;
            case INVERSE:
                buffer[x + (y / 8) * this->width()] ^= (1 << (y & 7));
                break;
        }
    }
}

void OLEDDisplay::clearPixel(int16_t x, int16_t y) {
    if (x >= 0 && x < this->width() && y >= 0 && y < this->height()) {
        switch (color) {
            case BLACK:
                buffer[x + (y >> 3) * this->width()] |= (1 << (y & 7));
                break;
            case WHITE:
                buffer[x + (y >> 3) * this->width()] &= ~(1 << (y & 7));
                break;
            case INVERSE:
                buffer[x + (y >> 3) * this->width()] ^= (1 << (y & 7));
                break;
        }
    }
}


// Bresenham's algorithm - thx wikipedia and Adafruit_GFX
void OLEDDisplay::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) {
        _swap_int16_t(x0, y0);
        _swap_int16_t(x1, y1);
    }

    if (x0 > x1) {
        _swap_int16_t(x0, x1);
        _swap_int16_t(y0, y1);
    }

    int16_t dx, dy;
    dx = x1 - x0;
    dy = abs(y1 - y0);

    int16_t err = dx / 2;
    int16_t ystep;

    if (y0 < y1) {
        ystep = 1;
    } else {
        ystep = -1;
    }

    for (; x0 <= x1; x0++) {
        if (steep) {
            setPixel(y0, x0);
        } else {
            setPixel(x0, y0);
        }
        err -= dy;
        if (err < 0) {
            y0 += ystep;
            err += dx;
        }
    }
}

void OLEDDisplay::drawRect(int16_t x, int16_t y, int16_t width, int16_t height) {
    drawHorizontalLine(x, y, width);
    drawVerticalLine(x, y, height);
    drawVerticalLine(x + width - 1, y, height);
    drawHorizontalLine(x, y + height - 1, width);
}

void OLEDDisplay::fillRect(int16_t xMove, int16_t yMove, int16_t width, int16_t height) {
    for (int16_t x = xMove; x < xMove + width; x++) {
        drawVerticalLine(x, yMove, height);
    }
}

void OLEDDisplay::drawCircle(int16_t x0, int16_t y0, int16_t radius) {
    int16_t x = 0, y = radius;
    int16_t dp = 1 - radius;
    do {
        if (dp < 0)
            dp = dp + (x++) * 2 + 3;
        else
            dp = dp + (x++) * 2 - (y--) * 2 + 5;

        setPixel(x0 + x, y0 + y);     //For the 8 octants
        setPixel(x0 - x, y0 + y);
        setPixel(x0 + x, y0 - y);
        setPixel(x0 - x, y0 - y);
        setPixel(x0 + y, y0 + x);
        setPixel(x0 - y, y0 + x);
        setPixel(x0 + y, y0 - x);
        setPixel(x0 - y, y0 - x);

    } while (x < y);

    setPixel(x0 + radius, y0);
    setPixel(x0, y0 + radius);
    setPixel(x0 - radius, y0);
    setPixel(x0, y0 - radius);
}

void OLEDDisplay::drawCircleQuads(int16_t x0, int16_t y0, int16_t radius, uint8_t quads) {
    int16_t x = 0, y = radius;
    int16_t dp = 1 - radius;
    while (x < y) {
        if (dp < 0)
            dp = dp + (x++) * 2 + 3;
        else
            dp = dp + (x++) * 2 - (y--) * 2 + 5;
        if (quads & 0x1) {
            setPixel(x0 + x, y0 - y);
            setPixel(x0 + y, y0 - x);
        }
        if (quads & 0x2) {
            setPixel(x0 - y, y0 - x);
            setPixel(x0 - x, y0 - y);
        }
        if (quads & 0x4) {
            setPixel(x0 - y, y0 + x);
            setPixel(x0 - x, y0 + y);
        }
        if (quads & 0x8) {
            setPixel(x0 + x, y0 + y);
            setPixel(x0 + y, y0 + x);
        }
    }
    if (quads & 0x1 && quads & 0x8) {
        setPixel(x0 + radius, y0);
    }
    if (quads & 0x4 && quads & 0x8) {
        setPixel(x0, y0 + radius);
    }
    if (quads & 0x2 && quads & 0x4) {
        setPixel(x0 - radius, y0);
    }
    if (quads & 0x1 && quads & 0x2) {
        setPixel(x0, y0 - radius);
    }
}

void OLEDDisplay::fillCircle(int16_t x0, int16_t y0, int16_t radius) {
    int16_t x = 0, y = radius;
    int16_t dp = 1 - radius;
    do {
        if (dp < 0)
            dp = dp + (x++) * 2 + 3;
        else
            dp = dp + (x++) * 2 - (y--) * 2 + 5;

        drawHorizontalLine(x0 - x, y0 - y, 2 * x);
        drawHorizontalLine(x0 - x, y0 + y, 2 * x);
        drawHorizontalLine(x0 - y, y0 - x, 2 * y);
        drawHorizontalLine(x0 - y, y0 + x, 2 * y);


    } while (x < y);
    drawHorizontalLine(x0 - radius, y0, 2 * radius);

}

void OLEDDisplay::drawHorizontalLine(int16_t x, int16_t y, int16_t length) {
    if (y < 0 || y >= this->height()) { return; }

    if (x < 0) {
        length += x;
        x = 0;
    }

    if ((x + length) > this->width()) {
        length = (this->width() - x);
    }

    if (length <= 0) { return; }

    uint8_t *bufferPtr = buffer;
    bufferPtr += (y >> 3) * this->width();
    bufferPtr += x;

    uint8_t drawBit = 1 << (y & 7);

    switch (color) {
        case WHITE:
            while (length--) {
                *bufferPtr++ |= drawBit;
            };
            break;
        case BLACK:
            drawBit = ~drawBit;
            while (length--) {
                *bufferPtr++ &= drawBit;
            };
            break;
        case INVERSE:
            while (length--) {
                *bufferPtr++ ^= drawBit;
            };
            break;
    }
}

void OLEDDisplay::drawVerticalLine(int16_t x, int16_t y, int16_t length) {
    if (x < 0 || x >= this->width()) return;

    if (y < 0) {
        length += y;
        y = 0;
    }

    if ((y + length) > this->height()) {
        length = (this->height() - y);
    }

    if (length <= 0) return;


    uint8_t yOffset = y & 7;
    uint8_t drawBit;
    uint8_t *bufferPtr = buffer;

    bufferPtr += (y >> 3) * this->width();
    bufferPtr += x;

    if (yOffset) {
        yOffset = 8 - yOffset;
        drawBit = ~(0xFF >> (yOffset));

        if (length < yOffset) {
            drawBit &= (0xFF >> (yOffset - length));
        }

        switch (color) {
            case WHITE:
                *bufferPtr |= drawBit;
                break;
            case BLACK:
                *bufferPtr &= ~drawBit;
                break;
            case INVERSE:
                *bufferPtr ^= drawBit;
                break;
        }

        if (length < yOffset) return;

        length -= yOffset;
        bufferPtr += this->width();
    }

    if (length >= 8) {
        switch (color) {
            case WHITE:
            case BLACK:
                drawBit = (color == WHITE) ? 0xFF : 0x00;
                do {
                    *bufferPtr = drawBit;
                    bufferPtr += this->width();
                    length -= 8;
                } while (length >= 8);
                break;
            case INVERSE:
                do {
                    *bufferPtr = ~(*bufferPtr);
                    bufferPtr += this->width();
                    length -= 8;
                } while (length >= 8);
                break;
        }
    }

    if (length > 0) {
        drawBit = (1 << (length & 7)) - 1;
        switch (color) {
            case WHITE:
                *bufferPtr |= drawBit;
                break;
            case BLACK:
                *bufferPtr &= ~drawBit;
                break;
            case INVERSE:
                *bufferPtr ^= drawBit;
                break;
        }
    }
}

void OLEDDisplay::drawProgressBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t progress) {
    uint16_t radius = height / 2;
    uint16_t xRadius = x + radius;
    uint16_t yRadius = y + radius;
    uint16_t doubleRadius = 2 * radius;
    uint16_t innerRadius = radius - 2;

    setColor(WHITE);
    drawCircleQuads(xRadius, yRadius, radius, 0b00000110);
    drawHorizontalLine(xRadius, y, width - doubleRadius + 1);
    drawHorizontalLine(xRadius, y + height, width - doubleRadius + 1);
    drawCircleQuads(x + width - radius, yRadius, radius, 0b00001001);

    uint16_t maxProgressWidth = (width - doubleRadius + 1) * progress / 100;

    fillCircle(xRadius, yRadius, innerRadius);
    fillRect(xRadius + 1, y + 2, maxProgressWidth, height - 3);
    fillCircle(xRadius + maxProgressWidth, yRadius, innerRadius);
}

void OLEDDisplay::drawFastImage(int16_t xMove, int16_t yMove, int16_t width, int16_t height, const uint8_t *image) {
    drawInternal(xMove, yMove, width, height, image, 0, 0, 0, 0, this->width(), this->height());
}

void OLEDDisplay::drawXbm(int16_t xMove, int16_t yMove, int16_t width, int16_t height, const uint8_t *xbm) {
    int16_t widthInXbm = (width + 7) / 8;
    uint8_t data = 0;

    for (int16_t y = 0; y < height; y++) {
        for (int16_t x = 0; x < width; x++) {
            if (x & 7) {
                data >>= 1; // Move a bit
            } else {  // Read new data every 8 bit
                data = pgm_read_byte(xbm + (x / 8) + y * widthInXbm);
            }
            // if there is a bit draw it
            if (data & 0x01) {
                setPixel(xMove + x, yMove + y);
            }
        }
    }
}

void OLEDDisplay::drawIco16x16(int16_t xMove, int16_t yMove, const char *ico, bool inverse) {
    uint16_t data;

    for (int16_t y = 0; y < 16; y++) {
        data = pgm_read_byte(ico + (y << 1)) + (pgm_read_byte(ico + (y << 1) + 1) << 8);
        for (int16_t x = 0; x < 16; x++) {
            if ((data & 0x01) ^ inverse) {
                setPixelColor(xMove + x, yMove + y, WHITE);
            } else {
                setPixelColor(xMove + x, yMove + y, BLACK);
            }
            data >>= 1; // Move a bit
        }
    }
}

void OLEDDisplay::drawStringInternal(int16_t xMove, int16_t yMove, char *text, uint16_t textLength, uint16_t textWidth,
                                     short width, short height, short offsetX, short offsetY) {
    uint8_t textHeight = pgm_read_byte(fontData + HEIGHT_POS);
    uint8_t firstChar = pgm_read_byte(fontData + FIRST_CHAR_POS);
    uint16_t sizeOfJumpTable = pgm_read_byte(fontData + CHAR_NUM_POS) * JUMPTABLE_BYTES;

    uint16_t cursorX = 0;

    // Don't draw anything if it is not on the screen.
    if (width <= 0 || xMove + offsetX + textWidth < 0 || xMove + offsetX > this->width()) { return; }
    if (height <= 0 || yMove + offsetY + textHeight < 0 || yMove + offsetY > this->height()) { return; }

    for (uint16_t j = 0; j < textLength; j++) {
        short render_x = xMove + cursorX + offsetX;
        short render_y = yMove + offsetY;

        if (render_x >= xMove + width || render_y >= yMove + height)
            break;

        uint8_t code = text[j];

        // skip invalid characters
        if (code < firstChar)
            continue;

        uint8_t charCode = code - firstChar;

        // 4 Bytes per char code
        uint8_t msbJumpToChar = pgm_read_byte(
                fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES);                  // MSB  \ JumpAddress
        uint8_t lsbJumpToChar = pgm_read_byte(
                fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES + JUMPTABLE_LSB);   // LSB /
        uint8_t charByteSize = pgm_read_byte(
                fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES + JUMPTABLE_SIZE);  // Size
        uint8_t currentCharWidth = pgm_read_byte(
                fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES + JUMPTABLE_WIDTH); // Width

        cursorX += currentCharWidth;

        int xMin = 0, yMin = 0;

        if (cursorX - currentCharWidth + offsetX < 0) {
            if (cursorX + offsetX > 0)
                xMin = abs(cursorX + offsetX - currentCharWidth);
            else
                continue;
        }

        // yMove = 200
        // offsetY = 17
        // renderY = 217
        // height = 20

        // renderY + textHeight (18) = 207

        if (render_y < yMove) {
                if (render_y + textHeight > yMove)
                    yMin = yMove - render_y;
                else
                    continue;
            }

        int xMax = width - (cursorX - currentCharWidth + offsetX);

        if (cursorX - currentCharWidth + offsetX >= width)
            break;

        if (render_y > yMove + height)
            break;

        int yMax = abs(height - (render_y - yMove));


        // skip characters that we dont want to render
        if (msbJumpToChar == 255 && lsbJumpToChar == 255)
            continue;


        // Get the position of the char data
        uint16_t charDataPosition = JUMPTABLE_START + sizeOfJumpTable + ((msbJumpToChar << 8) + lsbJumpToChar);

        drawInternal(render_x, render_y, currentCharWidth, textHeight, fontData, charDataPosition, charByteSize, xMin, yMin, xMax, yMax);
    }
}

void OLEDDisplay::drawString(int16_t xMove, int16_t yMove, String strUser, short width, short height, short offsetX,
                             short offsetY) {
    uint16_t lineHeight = pgm_read_byte(fontData + HEIGHT_POS);

    // char* text must be freed!
    char *text = utf8ascii(strUser);

    // set width/height to display dimensions when they are equal 0
    if (width == 0) width = this->width();
    if (height == 0) height = this->height();

    short yOffset = textAlignment == TEXT_ALIGN_CENTER_BOTH ? -getStringHeight(strUser) >> 1 : 0;

    char *textPart;
    short line;

    for (line = 0, textPart = strtok(text, "\n"); textPart != NULL; line++, textPart = strtok(NULL, "\n")) {
        short textWidth = getStringWidth(textPart);
        short xPos = xMove;

        switch (textAlignment) {
            case TEXT_ALIGN_CENTER_BOTH:
            case TEXT_ALIGN_CENTER:
                xPos -= textWidth >> 1; // divide by 2
                break;
            case TEXT_ALIGN_RIGHT:
                xPos -= textWidth;
                break;
            case TEXT_ALIGN_LEFT:
                break;
        }

        drawStringInternal(xPos, yMove + yOffset, textPart, strlen(textPart), textWidth, width, height, offsetX,
                           offsetY);

        offsetY += lineHeight;
    }

    free(text);
}

int OLEDDisplay::calculateScrollPositionHorizontal(int renderWidth, String string,
                                                   OLEDDISPLAY_ANIMATION_PROPERTIES properties) {
    int scroll_distance = this->getStringWidth(string) - renderWidth + 2 * properties.margin;
    long timeVal = millis() * properties.speed / 50;
    bool moveLeft = timeVal / (scroll_distance + 1) % 2 == 0;
    int position_x;

    position_x = round(
            getEasingProgress(properties.easing, timeVal % (scroll_distance + 1), scroll_distance) * scroll_distance);

    return moveLeft ? properties.margin - position_x : properties.margin + position_x - scroll_distance;
}

int OLEDDisplay::calculateScrollPositionVertical(int renderHeight, String string,
                                                 OLEDDISPLAY_ANIMATION_PROPERTIES properties) {
    int scroll_distance = this->getStringHeight(string) - renderHeight + 2 * properties.margin;
    long timeVal = millis() * properties.speed / 50;
    bool moveUp = timeVal / (scroll_distance + 1) % 2 == 0;
    int position_y;

    position_y = round(
            getEasingProgress(properties.easing, timeVal % (scroll_distance + 1), scroll_distance) * scroll_distance);

    return moveUp ? properties.margin - position_y : properties.margin + position_y - scroll_distance;
}

void OLEDDisplay::drawStringHorizontalScrolling(int16_t xMove, int16_t yMove, String strUser, short width, short height,
                                                OLEDDISPLAY_ANIMATION_PROPERTIES properties) {
    if (width == 0) width = this->width() - xMove;
    if (height == 0) height = this->height() - yMove;

    drawString(xMove, yMove, strUser, width, height, calculateScrollPositionHorizontal(width, strUser, properties));
}

void OLEDDisplay::drawStringVerticalScrolling(int16_t xMove, int16_t yMove, String strUser, short width, short height,
                                              OLEDDISPLAY_ANIMATION_PROPERTIES properties) {
    if (width == 0) width = this->width() - xMove;
    if (height == 0) height = this->height() - yMove;

    drawString(xMove, yMove, strUser, width, height, 0, calculateScrollPositionVertical(height, strUser, properties));
}


void OLEDDisplay::drawStringMaxWidth(int16_t xMove, int16_t yMove, uint16_t maxLineWidth, String strUser) {
    uint16_t firstChar = pgm_read_byte(fontData + FIRST_CHAR_POS);
    uint16_t lineHeight = pgm_read_byte(fontData + HEIGHT_POS);

    char *text = utf8ascii(strUser);

    uint16_t length = strlen(text);
    uint16_t lastDrawnPos = 0;
    uint16_t lineNumber = 0;
    uint16_t strWidth = 0;

    uint16_t preferredBreakpoint = 0;
    uint16_t widthAtBreakpoint = 0;

    for (uint16_t i = 0; i < length; i++) {
        strWidth += pgm_read_byte(
                fontData + JUMPTABLE_START + (text[i] - firstChar) * JUMPTABLE_BYTES + JUMPTABLE_WIDTH);

        // Always try to break on a space or dash
        if (text[i] == ' ' || text[i] == '-') {
            preferredBreakpoint = i;
            widthAtBreakpoint = strWidth;
        }

        if (strWidth >= maxLineWidth) {
            if (preferredBreakpoint == 0) {
                preferredBreakpoint = i;
                widthAtBreakpoint = strWidth;
            }
            drawStringInternal(xMove, yMove + (lineNumber++) * lineHeight, &text[lastDrawnPos],
                               preferredBreakpoint - lastDrawnPos, widthAtBreakpoint);
            lastDrawnPos = preferredBreakpoint + 1;
            // It is possible that we did not draw all letters to i so we need
            // to account for the width of the chars from `i - preferredBreakpoint`
            // by calculating the width we did not draw yet.
            strWidth = strWidth - widthAtBreakpoint;
            preferredBreakpoint = 0;
        }
    }

    // Draw last part if needed
    if (lastDrawnPos < length) {
        drawStringInternal(xMove, yMove + lineNumber * lineHeight, &text[lastDrawnPos], length - lastDrawnPos,
                           getStringWidth(&text[lastDrawnPos], length - lastDrawnPos));
    }

    free(text);
}

uint16_t OLEDDisplay::getStringHeight(String text) {
    uint16_t lineHeight = pgm_read_byte(fontData + HEIGHT_POS);

    uint16_t lb = 1;
    // Find number of linebreaks in text
    for (uint16_t i = 0; text[i] != 0; i++) {
        lb += (text[i] == '\n');
    }

    return lb * lineHeight;
}

uint16_t OLEDDisplay::getStringWidth(const char *text, uint16_t length) {
    uint16_t firstChar = pgm_read_byte(fontData + FIRST_CHAR_POS);

    uint16_t stringWidth = 0;
    uint16_t maxWidth = 0;

    while (length--) {
        stringWidth += pgm_read_byte(
                fontData + JUMPTABLE_START + (text[length] - firstChar) * JUMPTABLE_BYTES + JUMPTABLE_WIDTH);
        if (text[length] == '\n') {
            maxWidth = max(maxWidth, stringWidth);
            stringWidth = 0;
        }
    }

    return max(maxWidth, stringWidth);
}

uint16_t OLEDDisplay::getStringWidth(String strUser) {
    char *text = utf8ascii(strUser);
    uint16_t width = getStringWidth(text, strlen(text));
    free(text);
    return width;
}

void OLEDDisplay::setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT textAlignment) {
    this->textAlignment = textAlignment;
}

void OLEDDisplay::setFont(const uint8_t *fontData) {
    this->fontData = fontData;
}

void OLEDDisplay::displayOn(void) {
    sendCommand(DISPLAYON);
}

void OLEDDisplay::displayOff(void) {
    sendCommand(DISPLAYOFF);
}

void OLEDDisplay::invertDisplay(void) {
    sendCommand(INVERTDISPLAY);
}

void OLEDDisplay::normalDisplay(void) {
    sendCommand(NORMALDISPLAY);
}

void OLEDDisplay::setContrast(uint8_t contrast, uint8_t precharge, uint8_t comdetect) {
    sendCommand(SETPRECHARGE); //0xD9
    sendCommand(precharge); //0xF1 default, to lower the contrast, put 1-1F
    sendCommand(SETCONTRAST);
    sendCommand(contrast); // 0-255
    sendCommand(SETVCOMDETECT); //0xDB, (additionally needed to lower the contrast)
    sendCommand(comdetect);    //0x40 default, to lower the contrast, put 0
    sendCommand(DISPLAYALLON_RESUME);
    sendCommand(NORMALDISPLAY);
    sendCommand(DISPLAYON);
}

void OLEDDisplay::setBrightness(uint8_t brightness) {
    uint8_t contrast = brightness * 1.171;

    // Magic values to get a smooth/ step-free transition
    if (brightness >= 128) {
        contrast -= 43;
    }

    uint8_t precharge = brightness == 0 ? 0 : 241;
    uint8_t comdetect = brightness / 8;

    setContrast(contrast, precharge, comdetect);
}

void OLEDDisplay::resetOrientation() {
    sendCommand(SEGREMAP);
    sendCommand(COMSCANINC);           //Reset screen rotation or mirroring
}

void OLEDDisplay::flipScreenVertically() {
    sendCommand(SEGREMAP | 0x01);
    sendCommand(COMSCANDEC);           //Rotate screen 180 Deg
}

void OLEDDisplay::mirrorScreen() {
    sendCommand(SEGREMAP);
    sendCommand(COMSCANDEC);           //Mirror screen
}

void OLEDDisplay::clear(void) {
    memset(buffer, 0, displayBufferSize);
}

void OLEDDisplay::drawLogBuffer(uint16_t xMove, uint16_t yMove) {
    // Always align left
    setTextAlignment(TEXT_ALIGN_LEFT);

    drawString(xMove, yMove, String(this->logBuffer));
}

uint16_t OLEDDisplay::getWidth(void) {
    return displayWidth;
}

uint16_t OLEDDisplay::getHeight(void) {
    return displayHeight;
}

bool OLEDDisplay::setLogBuffer(uint16_t lines, uint16_t chars) {
    if (logBuffer != NULL) free(logBuffer);
    uint16_t size = lines * chars;
    if (size > 0) {
        this->logBufferLine = 0;      // Lines printed
        this->logBufferFilled = 0;      // Nothing stored yet
        this->logBufferMaxLines = lines;  // Lines max printable
        this->logBufferSize = size;   // Total number of characters the buffer can hold
        this->logBuffer = (char *) malloc(size * sizeof(uint8_t));
        if (!this->logBuffer) {
            DEBUG_OLEDDISPLAY("[OLEDDISPLAY][setLogBuffer] Not enough memory to create log buffer\n");
            return false;
        }
    }
    return true;
}

size_t OLEDDisplay::write(uint8_t c) {
    if (this->logBufferSize > 0) {
        // Don't waste space on \r\n line endings, dropping \r
        if (c == 13) return 1;

        // convert UTF-8 character to font table index
        c = (this->fontTableLookupFunction)(c);
        // drop unknown character
        if (c == 0) return 1;

        bool maxLineNotReached = this->logBufferLine < this->logBufferMaxLines;
        bool bufferNotFull = this->logBufferFilled < this->logBufferSize;

        // Can we write to the buffer?
        if (bufferNotFull && maxLineNotReached) {
            this->logBuffer[logBufferFilled] = c;
            this->logBufferFilled++;
            // Keep track of lines written
            if (c == 10) this->logBufferLine++;
        } else {
            // Max line number is reached
            if (!maxLineNotReached) this->logBufferLine--;

            // Find the end of the first line
            uint16_t firstLineEnd = 0;
            for (uint16_t i = 0; i < this->logBufferFilled; i++) {
                if (this->logBuffer[i] == 10) {
                    // Include last char too
                    firstLineEnd = i + 1;
                    break;
                }
            }
            // If there was a line ending
            if (firstLineEnd > 0) {
                // Calculate the new logBufferFilled value
                this->logBufferFilled = logBufferFilled - firstLineEnd;
                // Now we move the lines infront of the buffer
                memcpy(this->logBuffer, &this->logBuffer[firstLineEnd], logBufferFilled);
            } else {
                // Let's reuse the buffer if it was full
                if (!bufferNotFull) {
                    this->logBufferFilled = 0;
                }// else {
                //  Nothing to do here
                //}
            }
            write(c);
        }
    }
    // We are always writing all uint8_t to the buffer
    return 1;
}

size_t OLEDDisplay::write(const char *str) {
    if (str == NULL) return 0;
    size_t length = strlen(str);
    for (size_t i = 0; i < length; i++) {
        write(str[i]);
    }
    return length;
}

#ifdef __MBED__
int OLEDDisplay::_putc(int c) {

    if (!fontData)
        return 1;
    if (!logBufferSize) {
        uint8_t textHeight = pgm_read_byte(fontData + HEIGHT_POS);
        uint16_t lines =  this->displayHeight / textHeight;
        uint16_t chars =   2 * (this->displayWidth / textHeight);

        if (this->displayHeight % textHeight)
            lines++;
        if (this->displayWidth % textHeight)
            chars++;
        setLogBuffer(lines, chars);
    }

    return this->write((uint8_t)c);
}
#endif

// Private functions
void OLEDDisplay::setGeometry(OLEDDISPLAY_GEOMETRY g, uint16_t width, uint16_t height) {
    this->geometry = g;
    switch (g) {
        case GEOMETRY_128_64:
            this->displayWidth = 128;
            this->displayHeight = 64;
            break;
        case GEOMETRY_128_32:
            this->displayWidth = 128;
            this->displayHeight = 32;
            break;
        case GEOMETRY_RAWMODE:
            this->displayWidth = width > 0 ? width : 128;
            this->displayHeight = height > 0 ? height : 64;
            break;
    }
    this->displayBufferSize = displayWidth * displayHeight / 8;
}

void OLEDDisplay::sendInitCommands(void) {
    if (geometry == GEOMETRY_RAWMODE)
        return;
    sendCommand(DISPLAYOFF);
    sendCommand(SETDISPLAYCLOCKDIV);
    sendCommand(0xF0); // Increase speed of the display max ~96Hz
    sendCommand(SETMULTIPLEX);
    sendCommand(this->height() - 1);
    sendCommand(SETDISPLAYOFFSET);
    sendCommand(0x00);
    sendCommand(SETSTARTLINE);
    sendCommand(CHARGEPUMP);
    sendCommand(0x14);
    sendCommand(MEMORYMODE);
    sendCommand(0x00);
    sendCommand(SEGREMAP);
    sendCommand(COMSCANINC);
    sendCommand(SETCOMPINS);

    if (geometry == GEOMETRY_128_64) {
        sendCommand(0x12);
    } else if (geometry == GEOMETRY_128_32) {
        sendCommand(0x02);
    }

    sendCommand(SETCONTRAST);

    if (geometry == GEOMETRY_128_64) {
        sendCommand(0xCF);
    } else if (geometry == GEOMETRY_128_32) {
        sendCommand(0x8F);
    }

    sendCommand(SETPRECHARGE);
    sendCommand(0xF1);
    sendCommand(SETVCOMDETECT); //0xDB, (additionally needed to lower the contrast)
    sendCommand(0x40);            //0x40 default, to lower the contrast, put 0
    sendCommand(DISPLAYALLON_RESUME);
    sendCommand(NORMALDISPLAY);
    sendCommand(0x2e);            // stop scroll
    sendCommand(DISPLAYON);
}

void inline OLEDDisplay::drawInternal(int16_t xMove, int16_t yMove, int16_t width, int16_t height,
                                      const uint8_t *data, uint16_t offset, uint16_t bytesInData,
                                      uint16_t xMin, uint16_t yMin, uint16_t xMax, uint16_t yMax) {
    if (width <= 0 || height <= 0) return;
    if (yMove + height < 0 || yMove > this->height()) return;
    if (xMove + width < 0 || xMove > this->width()) return;

    if (xMax < 0 || yMax < 0) return;

    uint8_t rasterHeight = 1 + ((height - 1) >> 3); // fast ceil(height / 8.0)
    int8_t yOffset = yMove & 7;

    bytesInData = bytesInData == 0 ? width * rasterHeight : bytesInData;

    int16_t initYMove = yMove;
    int8_t initYOffset = yOffset;

    uint8_t cropBytes[rasterHeight];

    for(int i=0; i < rasterHeight; i++)
        cropBytes[i] = 0;

    for(int p = 0; p < rasterHeight * 8; p++)
        if(yMin > p || yMax < p)
            cropBytes[p/8] |= 1 << (p%8);

    for (uint16_t i = 0; i < bytesInData; i++) {
        #ifndef __MBED__
        yield(); // we do this on start to avoid skipping yields
        #endif

        uint16_t charX = i / rasterHeight;

        // Reset if next horizontal drawing phase is started.
        if (i % rasterHeight == 0) {
            yMove = initYMove;
            yOffset = initYOffset;
        }

        // skip horizontal pixels that are out of render range
        if (charX < xMin || charX >= xMax)
            continue;

        uint8_t currentByte = pgm_read_byte(data + offset + i);

        int16_t xPos = xMove + charX;
        int16_t yPos = ((yMove >> 3) + i % rasterHeight) * this->width();

        int16_t dataPos = xPos + yPos;

        // cancel when buffer limit is reached or rendering off screen
        if(dataPos >= displayBufferSize || xPos >= this->width())
            break;

        if (dataPos < 0 || xPos < 0)
            continue;

        currentByte &= ~(cropBytes[i % rasterHeight]);

        switch (this->color) {
            case WHITE:   buffer[dataPos] |= currentByte << yOffset; break;
            case BLACK:   buffer[dataPos] &= ~(currentByte << yOffset); break;
            case INVERSE: buffer[dataPos] ^= currentByte << yOffset; break;
        }

        if(yOffset < 0){
            // Prepare for next iteration
            yMove -= 8;   // move one block up
            yOffset += 8; // and set new yOffset
        }
        else if (dataPos + this->width() < displayBufferSize) {
            switch (this->color) {
                case WHITE:   buffer[dataPos + this->width()] |= currentByte >> (8 - yOffset); break;
                case BLACK:   buffer[dataPos + this->width()] &= ~(currentByte >> (8 - yOffset)); break;
                case INVERSE: buffer[dataPos + this->width()] ^= currentByte >> (8 - yOffset); break;
            }
        }

    }
}

// You need to free the char!
char *OLEDDisplay::utf8ascii(String str) {
    uint16_t k = 0;
    uint16_t length = str.length() + 1;

    // Copy the string into a char array
    char *s = (char *) malloc(length * sizeof(char));
    if (!s) {
        DEBUG_OLEDDISPLAY("[OLEDDISPLAY][utf8ascii] Can't allocate another char array. Drop support for UTF-8.\n");
        return (char *) str.c_str();
    }
    str.toCharArray(s, length);

    length--;

    for (uint16_t i = 0; i < length; i++) {
        char c = (this->fontTableLookupFunction)(s[i]);
        if (c != 0) {
            s[k++] = c;
        }
    }

    s[k] = 0;

    // This will leak 's' be sure to free it in the calling function.
    return s;
}

void OLEDDisplay::setFontTableLookupFunction(FontTableLookupFunction function) {
    this->fontTableLookupFunction = function;
}


char DefaultFontTableLookup(const uint8_t ch) {
    // UTF-8 to font table index converter
    // Code form http://playground.arduino.cc/Main/Utf8ascii
    static uint8_t LASTCHAR;

    if (ch < 128) { // Standard ASCII-set 0..0x7F handling
        LASTCHAR = 0;
        return ch;
    }

    uint8_t last = LASTCHAR;   // get last char
    LASTCHAR = ch;

    switch (last) {    // conversion depnding on first UTF8-character
        case 0xC2:
            return (uint8_t) ch;
        case 0xC3:
            return (uint8_t) (ch | 0xC0);
        case 0x82:
            if (ch == 0xAC) return (uint8_t) 0x80;    // special case Euro-symbol
    }

    return (uint8_t) 0; // otherwise: return zero, if character has to be ignored
}
