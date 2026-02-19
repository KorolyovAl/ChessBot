#pragma once

#include <QString>
#include <QDebug>
#include "../engine_core/board_state/bitboard.h"

/**********************************************
* Prints a bitboard using qDebug.
* Appears in debugger output and Qt logs.
* Upper-left is a8, bottom-right is h1.
**********************************************/
inline void PrintBitboard(Bitboard bb) {
    QString result;
    for (int y = 7; y >= 0; --y) {
        for (int x = 0; x < 8; ++x) {
            int square = y * 8 + x;;
            result += (BOp::GetBit(bb, square) ? "1 " : ". ");
        }
        result += '\n';
    }
    qDebug().noquote() << "\n" << result;
}


/*

8 | 56 57 58 59 60 61 62 63
7 | 48 49 50 51 52 53 54 55
6 | 40 41 42 43 44 45 46 47
5 | 32 33 34 35 36 37 38 39
4 | 24 25 26 27 28 29 30 31
3 | 16 17 18 19 20 21 22 23
2 | 8  9  10 11 12 13 14 15
1 | 0  1  2  3  4  5  6  7
    a  b  c  d  e  f  g  h

 */
