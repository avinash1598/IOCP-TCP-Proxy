///////////////////////////////////////////////////////////////////////////////
// @File Name:     Logger.cpp                                                //
// @Version:       0.0.1                                                     //
// @Description:   For Logging into file                                     //
//                                                                           // 
// Detail Description:                                                       //
// Implemented complete logging mechanism, Supporting multiple logging type  //
// like as file based logging, console base logging etc. It also supported   //
// for different log type.                                                   //
//                                                                           //
// Thread Safe logging mechanism. Compatible with VC++ (Windows platform)   //
// as well as G++ (Linux platform)                                           //
//                                                                           //
// Supported Log Type: ERROR, ALARM, ALWAYS, INFO, BUFFER, TRACE, DEBUG      //
//                                                                           //
// No control for ERROR, ALRAM and ALWAYS messages. These type of messages   //
// should be always captured.                                                //
//                                                                           //
// BUFFER log type should be use while logging raw buffer or raw messages    //
//                                                                           //
// Having direct interface as well as C++ Singleton inface. can use          //
// whatever interface want.                                                   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

// C++ Header File(s)
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cstdlib>

// Code Specific Header Files(s)
#include "Logger.h"

using namespace std;
using namespace CPlusPlusLogging;

Logger* Logger::m_Instance = 0;

// Log file name. File name should be change from here only
const string logFileName = "MdProxy.log";

Logger::Logger()
{
   m_File.open(logFileName.c_str(), ios::out|ios::app);
   m_LogLevel	= LOG_LEVEL::ENABLE_LOG;
   m_LogType	= LOG_TYPE::CONSOLE;

   // Initialize mutex
#ifdef WIN32
   InitializeCriticalSection(&m_Mutex);
#else
   int ret=0;
   ret = pthread_mutexattr_settype(&m_Attr, PTHREAD_MUTEX_ERRORCHECK_NP);
   if(ret != 0)
   {   
      printf("Logger::Logger() -- Mutex attribute not initialize!!\n");
      exit(0);
   }   
   ret = pthread_mutex_init(&m_Mutex,&m_Attr);
   if(ret != 0)
   {   
      printf("Logger::Logger() -- Mutex not initialize!!\n");
      exit(0);
   }   
#endif
}

Logger::~Logger()
{
   m_File.close();
#ifdef WIN32
   DeleteCriticalSection(&m_Mutex);
#else
   pthread_mutexattr_destroy(&m_Attr);
   pthread_mutex_destroy(&m_Mutex);
#endif
}

Logger* Logger::getInstance() throw ()
{
   if (m_Instance == 0) 
   {
      m_Instance = new Logger();
   }
   return m_Instance;
}

void Logger::lock()
{
#ifdef WIN32
   EnterCriticalSection(&m_Mutex);
#else
   pthread_mutex_lock(&m_Mutex);
#endif
}

void Logger::unlock()
{
#ifdef WIN32
   LeaveCriticalSection(&m_Mutex);
#else
   pthread_mutex_unlock(&m_Mutex);
#endif
}

void Logger::logIntoFile(std::string& data)
{
   lock();
   m_File << getCurrentTime() << "  " << data << endl;
   unlock();
}

void Logger::logOnConsole(std::string& data)
{
   lock();
   cout << getCurrentTime() << "  " << data << endl;
   unlock();
}

string Logger::getCurrentTime()
{
   char t_buffer[26] = {};
   string currTime;
   //Current date/time based on current time
   time_t now = time(0); 
   // Convert current time to string
   ctime_s(t_buffer, 26, &now);
   currTime.assign(t_buffer);

   // Last charactor of currentTime is "\n", so remove it
   string currentTime = currTime.substr(0, currTime.size()-1);
   return currentTime;
}

// Interface for Error Log
void Logger::error(const char* text, ...) throw()
{
   char buf[1024];
   string data;
   va_list args;

   va_start(args, text);

   vsprintf_s(buf, text, args);
   data.append("[ERROR]: ");
   data.append(buf);

   va_end(args);

   // ERROR must be capture
   if(m_LogType == LOG_TYPE::FILE_LOG)
   {
      logIntoFile(data);
   }
   else if(m_LogType == LOG_TYPE::CONSOLE)
   {
      logOnConsole(data);
   }
}

void Logger::error(std::string& text) throw()
{
   error(text.data());
}

void Logger::error(std::ostringstream& stream) throw()
{
   string text = stream.str();
   error(text.data());
}

// Interface for Alarm Log 
void Logger::alarm(const char* text, ...) throw()
{
    char buf[1024];
    string data;
    va_list args;

    va_start(args, text);

    vsprintf_s(buf, text, args);
    data.append("[ALARM]: ");
    data.append(buf);

    va_end(args);

   // ALARM must be capture
   if(m_LogType == LOG_TYPE::FILE_LOG)
   {
      logIntoFile(data);
   }
   else if(m_LogType == LOG_TYPE::CONSOLE)
   {
      logOnConsole(data);
   }
}

void Logger::alarm(std::string& text) throw()
{
   alarm(text.data());
}

void Logger::alarm(std::ostringstream& stream) throw()
{
   string text = stream.str();
   alarm(text.data());
}

// Interface for Always Log 
void Logger::always(const char* text, ...) throw()
{
   string data;
   data.append("[ALWAYS]: ");
   data.append(text);

   // No check for ALWAYS logs
   if(m_LogType == LOG_TYPE::FILE_LOG)
   {
      logIntoFile(data);
   }
   else if(m_LogType == LOG_TYPE::CONSOLE)
   {
      logOnConsole(data);
   }
}

void Logger::always(std::string& text) throw()
{
   always(text.data());
}

void Logger::always(std::ostringstream& stream) throw()
{
   string text = stream.str();
   always(text.data());
}

// Interface for Buffer Log 
void Logger::buffer(const char* text, ...) throw()
{
   // Buffer is the special case. So don't add log level
   // and timestamp in the buffer message. Just log the raw bytes.
   if((m_LogType == LOG_TYPE::FILE_LOG) && (m_LogLevel >= LOG_LEVEL::LOG_LEVEL_BUFFER))
   {
      lock();
      m_File << text << endl;
      unlock();
   }
   else if((m_LogType == LOG_TYPE::CONSOLE) && (m_LogLevel >= LOG_LEVEL::LOG_LEVEL_BUFFER))
   {
      cout << text << endl;
   }
}

void Logger::buffer(std::string& text) throw()
{
   buffer(text.data());
}

void Logger::buffer(std::ostringstream& stream) throw()
{
   string text = stream.str();
   buffer(text.data());
}

// Interface for Info Log
void Logger::info(const char* text, ...) throw()
{
   char buf[1024];
   string data;
   va_list args;

   va_start(args, text);
   
   vsprintf_s(buf, text, args);
   data.append("[INFO]: ");
   data.append(buf);
   
   va_end(args);

   if((m_LogType == LOG_TYPE::FILE_LOG) && (m_LogLevel >= LOG_LEVEL::LOG_LEVEL_INFO))
   {
      logIntoFile(data);
   }
   else if((m_LogType == LOG_TYPE::CONSOLE) && (m_LogLevel >= LOG_LEVEL::LOG_LEVEL_INFO))
   {
      logOnConsole(data);
   }
}

void Logger::info(std::string& text) throw()
{
   info(text.data());
}

void Logger::info(std::ostringstream& stream) throw()
{
   string text = stream.str();
   info(text.data());
}

// Interface for Trace Log
void Logger::trace(const char* text, ...) throw()
{
   string data;
   data.append("[TRACE]: ");
   data.append(text);

   if((m_LogType == LOG_TYPE::FILE_LOG) && (m_LogLevel >= LOG_LEVEL::LOG_LEVEL_TRACE))
   {
      logIntoFile(data);
   }
   else if((m_LogType == LOG_TYPE::CONSOLE) && (m_LogLevel >= LOG_LEVEL::LOG_LEVEL_TRACE))
   {
      logOnConsole(data);
   }
}

void Logger::trace(std::string& text) throw()
{
   trace(text.data());
}

void Logger::trace(std::ostringstream& stream) throw()
{
   string text = stream.str();
   trace(text.data());
}

// Interface for Debug Log
void Logger::debug(const char* text, ...) throw()
{
   char buf[1024];
   string data;
   va_list args;

   va_start(args, text);

   vsprintf_s(buf, text, args);
   data.append("[DEBUG]: ");
   data.append(buf);

   va_end(args);

   if((m_LogType == LOG_TYPE::FILE_LOG) && (m_LogLevel >= LOG_LEVEL::LOG_LEVEL_DEBUG))
   {
      logIntoFile(data);
   }
   else if((m_LogType == LOG_TYPE::CONSOLE) && (m_LogLevel >= LOG_LEVEL::LOG_LEVEL_DEBUG))
   {
      logOnConsole(data);
   }
}

void Logger::debug(std::string& text) throw()
{
   debug(text.data());
}

void Logger::debug(std::ostringstream& stream) throw()
{
   string text = stream.str();
   debug(text.data());
}

// Interfaces to control log levels
void Logger::updateLogLevel(LogLevel logLevel)
{
   m_LogLevel = logLevel;
}

// Enable all log levels
void Logger::enaleLog()
{
   m_LogLevel = LOG_LEVEL::ENABLE_LOG; 
}

// Disable all log levels, except error and alarm
void Logger:: disableLog()
{
   m_LogLevel = LOG_LEVEL::DISABLE_LOG;
}

// Interfaces to control log Types
void Logger::updateLogType(LogType logType)
{
   m_LogType = logType;
}

void Logger::enableConsoleLogging()
{
   m_LogType = LOG_TYPE::CONSOLE; 
}

void Logger::enableFileLogging()
{
   m_LogType = LOG_TYPE::FILE_LOG ;
}
