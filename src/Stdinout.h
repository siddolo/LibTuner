/*
    Stdinout.h - connect various character devices to standard streams
    Part of Arduino - http://www.arduino.cc/

    Copyright (c) 2014, 2015 Roger A. Krupski <rakrupski@verizon.net>

    Last update: 26 April 2015

    This library is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef STD_IN_OUT_H
#define STD_IN_OUT_H

#include <Stream.h>

class STDINOUT {
	public:
		void open (Stream &);
		void close (void);
	private:
		FILE *_fp = NULL;
		static int _putchar (char, FILE *);
		static int _getchar (FILE *);
};

extern STDINOUT STDIO; // Preinstantiate STDIO object

#endif

// end of Stdinout.h