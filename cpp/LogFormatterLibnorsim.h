#ifndef __LOGFORMATTERLIBNORSIM_H__
#define __LOGFORMATTERLIBNORSIM_H__

#include <cstdio>

#include "Logger.h"

class LogFormatterLibnorsim : public LogFormatter {
public:
	enum Color {
		RESET,
		LIGHT_RED,
		YELLOW,
		BROWN,
		LIGHT_GREEN,
		LIGHT_TEAL,
		WHITE
	};

	LogFormatterLibnorsim(bool colors)
	 : m_colors(colors) {}
	~LogFormatterLibnorsim() {}

	const char* format(Loglevel loglevel, const char* msg, bool raw) {
		if (raw)
			return (msg);
		if (m_colors) {
			snprintf(m_formatBuffer, LOGGER_PRINTBUFFER_SIZE, "Libnorsim: %s%s%s%s%s\n",
				getColor(getLoglevelColor(loglevel)),
				getLoglevelString(loglevel),
				getColor(Color::WHITE),
				msg,
				getColor(Color::RESET)
			);
			return (m_formatBuffer);
		}
		snprintf(m_formatBuffer, LOGGER_PRINTBUFFER_SIZE, "%s%s\n", getLoglevelString(loglevel), msg);
		return (m_formatBuffer);
	}

private:
	const char* getLoglevelString(Loglevel loglevel) {
		switch (loglevel) {
			case Loglevel::FATAL:
				return "FATAL:   ";
			case Loglevel::ERROR:
				return "ERROR:   ";
			case Loglevel::WARNING:
				return "WARNING: ";
			case Loglevel::INFO:
				return "INFO:    ";
			case Loglevel::DEBUG:
				return "DEBUG:   ";
			default:
				return ("");
		}
	}
	Color getLoglevelColor(Loglevel loglevel) {
		switch (loglevel) {
			case Loglevel::FATAL:
				return Color::LIGHT_RED;
			case Loglevel::ERROR:
				return Color::YELLOW;
			case Loglevel::WARNING:
				return Color::BROWN;
			case Loglevel::INFO:
				return Color::LIGHT_GREEN;
			case Loglevel::DEBUG:
				return Color::LIGHT_TEAL;
			default:
				return Color::RESET;
		}
	}
	const char* getColor(Color color) {
		switch (color) {
			case LIGHT_RED:
				return "\e[1;31m";
			case YELLOW:
				return "\e[1;33m";
			case BROWN:
				return "\e[0;33m";
			case LIGHT_GREEN:
				return "\e[1;32m";
			case LIGHT_TEAL:
				return "\e[1;36m";
			case WHITE:
				return "\e[1;37m";
			default:
				return ("\e[0m");
		}
	}

	bool m_colors;
	char m_formatBuffer[LOGGER_PRINTBUFFER_SIZE];
};

#endif // __LOGFORMATTERLIBNORSIM_H__