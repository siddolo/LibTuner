/*
    Stdinout.cpp - connect various character devices to standard streams
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

#include <Stdinout.h>

static Stream *_stream_ptr;

// connect stdio to stream
void STDINOUT::open (Stream &str)
{
	close();
	_stream_ptr = &str;
	_fp = fdevopen (_putchar, _getchar);
}

// disconnect stdio from stream
void STDINOUT::close (void)
{
	if (_fp) {
		fclose (_fp);
		_fp = NULL;
		_stream_ptr = NULL;
	}
}

// function that printf and related will use to write a char
int STDINOUT::_putchar (char c, FILE *_fp)
{
	if (c == '\n') { // \n sends crlf
		_putchar ((char) '\r', _fp);
	}

	_stream_ptr->write (c);
	return 0;
}

// Function that scanf and related will use to read a char
int STDINOUT::_getchar (FILE *_fp)
{
	while (! (_stream_ptr->available()));

	return (_stream_ptr->read());
}

STDINOUT STDIO; // Preinstantiate STDIO object

// end of stdinout.cpp