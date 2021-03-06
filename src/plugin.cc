/* Boilerplate for exporting a data type to the Analytics Engine.
 */

/* VA leaks a dependency upon _MAX_PATH */
#include <cstdlib>

/* Boost string algorithms */
#include <boost/algorithm/string.hpp>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>

#include <initguid.h>

#include "chromium/chromium_switches.hh"
#include "chromium/command_line.hh"
#include "chromium/logging.hh"
#include "chromium/logging_win.hh"
#include "psych.hh"

static const char* kPluginType = "psychPlugin";

// {A86E8172-4520-4043-B509-AF75C35326D3}
DEFINE_GUID(kLogProvider, 
0xa86e8172, 0x4520, 0x4043, 0xb5, 0x9, 0xaf, 0x75, 0xc3, 0x53, 0x26, 0xd3);

namespace
{
	class env_t
	{
	public:
		env_t (const char* varname)
		{
/* startup from clean string */
			CommandLine::Init (0, nullptr);
/* the program name */
			std::string command_line (kPluginType);
/* parameters from environment */
			char* buffer;
			size_t numberOfElements;
			const errno_t err = _dupenv_s (&buffer, &numberOfElements, varname);
			if (0 == err && numberOfElements > 0) {
				command_line.append (" ");
				command_line.append (buffer);
				free (buffer);
			}
			std::string log_path = GetLogFileName();
/* update command line */
			CommandLine::ForCurrentProcess()->ParseFromString (command_line);
/* forward onto logging */
			logging::InitLogging(
				log_path.c_str(),
				DetermineLogMode (*CommandLine::ForCurrentProcess()),
				logging::DONT_LOCK_LOG_FILE,
				logging::APPEND_TO_OLD_LOG_FILE,
				logging::ENABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS
				);
#ifndef USE_ETW_LOGGING
			logging::SetLogMessageHandler (log_handler);
#else
			logging::LogEventProvider::Initialize(kLogProvider);
#endif
		}
	protected:
		std::string GetLogFileName() {
			const std::string log_filename ("/psych.log");
			return log_filename;
		}

		logging::LoggingDestination DetermineLogMode (const CommandLine& command_line) {
#ifdef NDEBUG
			const logging::LoggingDestination kDefaultLoggingMode = logging::LOG_NONE;
#else
			const logging::LoggingDestination kDefaultLoggingMode = logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG;
#endif

			logging::LoggingDestination log_mode;
// Let --enable-logging=file force Vhayu and file logging, particularly useful for
// non-debug builds where otherwise you can't get logs on fault at all.
			if (command_line.GetSwitchValueASCII (switches::kEnableLogging) == "file")
				log_mode = logging::LOG_ONLY_TO_FILE;
			else
				log_mode = kDefaultLoggingMode;
			return log_mode;
		}

/* Vhayu log system wrapper */
		static bool log_handler (int severity, const char* file, int line, size_t message_start, const std::string& str)
		{
			int priority;
			switch (severity) {
			default:
			case logging::LOG_INFO:		priority = eMsgInfo; break;
			case logging::LOG_WARNING:	priority = eMsgLow; break;
			case logging::LOG_ERROR:	priority = eMsgMedium; break;
			case logging::LOG_FATAL:	priority = eMsgFatal; break;
			}
/* Yay, broken APIs */
			std::string str1 (boost::algorithm::trim_right_copy (str));
			MsgLog (priority, 0, const_cast<char*> ("%s"), str1.c_str());
/* allow additional log targets */
			return false;
		}
	};

	class winsock_t
	{
		bool initialized;
	public:
		winsock_t (unsigned majorVersion, unsigned minorVersion) :
			initialized (false)
		{
			WORD wVersionRequested = MAKEWORD (majorVersion, minorVersion);
			WSADATA wsaData;
			if (WSAStartup (wVersionRequested, &wsaData) != 0) {
				LOG(ERROR) << "WSAStartup returned " << WSAGetLastError();
				return;
			}
			if (LOBYTE (wsaData.wVersion) != majorVersion || HIBYTE (wsaData.wVersion) != minorVersion) {
				WSACleanup();
				LOG(ERROR) << "WSAStartup failed to provide requested version " << majorVersion << '.' << minorVersion;
				return;
			}
			initialized = true;
		}

		~winsock_t ()
		{
			if (initialized)
				WSACleanup();
		}
	};

	class factory_t : public vpf::ObjectFactory
	{
		env_t env;
		winsock_t winsock;
	public:
		factory_t() :
			env ("TR_DEBUG"),
			winsock (2, 2)
		{
			registerType (kPluginType);
		}

/* no API to unregister type. */

		virtual void* newInstance (const char* type) override
		{
			assert (0 == strcmp (kPluginType, type));
			return new psych::psych_t();
		}
	};

#ifndef CONFIG_PSYCH_AS_APPLICATION
	static factory_t g_factory_instance;
#endif

} /* anonymous namespace */

/* eof */
