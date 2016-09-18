#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <fstream>
#include <memory>
#include <mutex>

#ifndef LOGGER_PRINTBUFFER_SIZE
#error "LOGGER_PRINTBUFFER_SIZE undefined"
#endif

enum class Loglevel {
	ALWAYS = 0,
	FATAL,
	ERROR,
	WARNING,
	INFO,
	DEBUG
};

class LogFormatter {
public:
	LogFormatter() {}
	virtual ~LogFormatter() {}

	virtual const char* format(Loglevel loglevel, const char* msg, bool raw) = 0;
};

class Logger {
public:
	virtual ~Logger() {}

	virtual bool isOk() = 0;

	void log(const Loglevel level, const char *format, bool raw = false, ...) {
		std::lock_guard<std::mutex> lg(m_mutex);
		if (!m_logFormatter)
			return;
		va_list args;
		va_start(args, raw);

		if ((level == Loglevel::ALWAYS) || (level <= m_verbosity)) {
			vsnprintf(m_printBuffer, LOGGER_PRINTBUFFER_SIZE, format, args);
			write(m_logFormatter->format(level, m_printBuffer, raw));
		}
		va_end(args);
	}

	void setVerbosity(Loglevel verbosity) {
		std::lock_guard<std::mutex> lg(m_mutex);
		m_verbosity = verbosity;
	}

protected:
	Logger() {}

	virtual void write(const char *msg) = 0;

	char m_printBuffer[LOGGER_PRINTBUFFER_SIZE];
	LogFormatter* m_logFormatter;

private:

	Loglevel m_verbosity = Loglevel::DEBUG;
	std::mutex m_mutex;
};

class LoggerStdio : public Logger {
	friend class LoggerFactory;

public:
	~LoggerStdio() {}

	bool isOk() { return (true); }

private:
	LoggerStdio(LogFormatter *formatter) {
		m_logFormatter = formatter;
	}

	void write(const char *msg) { printf("%s", msg); fflush(stdout); }
};

class LoggerFile : public Logger {
	friend class LoggerFactory;

public:
	~LoggerFile() {
		if (m_outFile)
			fclose(m_outFile);
	}

	bool isOk() { return (m_ok); }
private:
	LoggerFile(LogFormatter *formatter, const char* path) {
		m_logFormatter = formatter;
		m_outFile = fopen(path, "a");
		if (m_outFile)
			m_ok = true;
	}

	void write(const char* msg) { fprintf(m_outFile, "%s", msg); fflush(m_outFile); }

	bool m_ok = false;
	FILE *m_outFile;
};

class LoggerFactory {
public:
	static Logger* createLoggerStdio(LogFormatter *formatter) {
		return (new LoggerStdio(formatter));
	}
	static Logger* createLoggerFile(LogFormatter *formatter, const char* file) {
		return (new LoggerFile(formatter, file));
	}
};

#endif // __LOGGER_H__
