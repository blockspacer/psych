/* A basic Velocity Analytics User-Plugin to exporting a new Tcl command and
 * periodically publishing out to ADH via RFA using RDM/MarketPrice.
 */

#ifndef __PSYCH_HH__
#define __PSYCH_HH__

#pragma once

#include <cstdint>
#include <unordered_map>

/* Boost Posix Time */
#include "boost/date_time/posix_time/posix_time.hpp"

/* Boost noncopyable base class. */
#include <boost/utility.hpp>

/* Boost threading. */
#include <boost/thread.hpp>

/* libcurl multi-interface */
#include <curl/multi.h>

/* RFA 7.2 */
#include <rfa.hh>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>

/* Microsoft wrappers */
#include "microsoft/timer.hh"

#include "config.hh"
#include "provider.hh"

namespace logging
{
	class LogEventProvider;
}

namespace psych
{
/* Performance Counters */
	enum {
		PSYCH_PC_TCL_QUERY_RECEIVED,
		PSYCH_PC_TIMER_QUERY_RECEIVED,
/*		PSYCH_PC_LAST_ACTIVITY,*/
/*		PSYCH_PC_TCL_SVC_TIME_MIN,*/
/*		PSYCH_PC_TCL_SVC_TIME_MEAN,*/
/*		PSYCH_PC_TCL_SVC_TIME_MAX,*/
		PSYCH_PC_HTTP_REQUEST_SENT,
		PSYCH_PC_HTTP_1XX_RECEIVED,		/* Informational */
		PSYCH_PC_HTTP_2XX_RECEIVED,		/* Success */
		PSYCH_PC_HTTP_3XX_RECEIVED,		/* Redirect */
		PSYCH_PC_HTTP_4XX_RECEIVED,		/* Client Error */
		PSYCH_PC_HTTP_5XX_RECEIVED,		/* Server Error */
		PSYCH_PC_HTTP_200_RECEIVED,		/* OK */
		PSYCH_PC_HTTP_304_RECEIVED,		/* Not Modified */
		PSYCH_PC_HTTP_MALFORMED,
		PSYCH_PC_HTTP_RETRIES_EXCEEDED,
		PSYCH_PC_HTTPD_CLOCK_DRIFT,		/* Webserver */
		PSYCH_PC_HTTP_CLOCK_DRIFT,		/* File system */
		PSYCH_PC_PSYCH_CLOCK_DRIFT,		/* MarketPsych */
/* marker */
		PSYCH_PC_MAX
	};

	class rfa_t;
	class provider_t;
	class snmp_agent_t;

/* libcurl traits */
	struct onepass_traits {
		static CURL* invalid() throw() {
			return nullptr;
		}
		static void close (CURL* value) throw()
		{
			curl_easy_cleanup (value);
		}
	};

	struct multipass_traits {
		static CURLM* invalid() throw() {
			return nullptr;
		}
		static void close (CURLM* value) throw()
		{
			curl_multi_cleanup (value);
		}
	};

/* libcurl connection */	
	class connection_t : boost::noncopyable
	{
	public:
		connection_t (const resource_t& resource_, const std::string& url_) :
			resource (resource_),
			url (url_),
			last_filetime (0L)
		{
		}

		const resource_t& resource;
		const std::string url;
		ms::unique_handle<CURL*, onepass_traits> handle;	/* non-copyable */
		char error[CURL_ERROR_SIZE];
		std::string data;
		boost::posix_time::ptime request_ptime;
		boost::posix_time::ptime httpd_ptime;
		long last_filetime;
	};

/* Basic state for each item stream. */
	class broadcast_stream_t : public item_stream_t
	{
	public:
		broadcast_stream_t (const resource_t& resource_) :
			resource (resource_)
		{
		}

		const resource_t& resource;
	};

	struct flex_filter_t
	{
		double bid_price;
		double ask_price;
	};

	class event_pump_t
	{
	public:
		event_pump_t (std::shared_ptr<rfa::common::EventQueue> event_queue) :
			event_queue_ (event_queue)
		{
		}

		void operator()()
		{
			while (event_queue_->isActive()) {
				event_queue_->dispatch (rfa::common::Dispatchable::InfiniteWait);
			}
		}

	private:
		std::shared_ptr<rfa::common::EventQueue> event_queue_;
	};

	class psych_t :
		public vpf::AbstractUserPlugin,
		public vpf::Command,
		boost::noncopyable
	{
	public:
		psych_t();
		virtual ~psych_t();

/* Plugin entry point. */
		virtual void init (const vpf::UserPluginConfig& config_);

/* Reset state suitable for recalling init(). */
		void clear();

/* Plugin termination point. */
		virtual void destroy();

/* Tcl entry point. */
		virtual int execute (const vpf::CommandInfo& cmdInfo, vpf::TCLCommandData& cmdData);

/* Configured period timer entry point. */
		void processTimer (void* closure);

/* Global list of all plugin instances.  AE owns pointer. */
		static std::list<psych_t*> global_list_;
		static boost::shared_mutex global_list_lock_;

	private:

/* Run core event loop. */
		void mainLoop();

		bool get_next_interval (FILETIME* ft);

		enum {
			QUERY_HTTP_KEEPALIVE = 1,
			QUERY_IF_MODIFIED_SINCE
		};
		int tclPsychRepublish (const vpf::CommandInfo& cmdInfo, vpf::TCLCommandData& cmdData);
		int tclPsychHardRepublish (const vpf::CommandInfo& cmdInfo, vpf::TCLCommandData& cmdData);
		bool httpPsychQuery (std::map<resource_t, std::shared_ptr<connection_t>, resource_compare_t>& connections, int flags);

/* Parse libcurl driven HTTP response. */
		bool processHttpResponse (connection_t* connection, boost::posix_time::ptime* open_time, boost::posix_time::ptime* close_time, std::vector<std::string>* columns, std::vector<std::pair<std::string, std::vector<double>>>* rows);

/* Broadcast out message. */
		bool sendRefresh (const resource_t& resource, const boost::posix_time::ptime& open_time, const boost::posix_time::ptime& close_time, const std::vector<std::string>& columns, const std::vector<std::pair<std::string, std::vector<double>>>& rows) throw (rfa::common::InvalidUsageException);

/* Generate DACS lock from entity code list */
		bool generatePELock (rfa::common::Buffer* buf, const rfa::common::RFA_Vector<unsigned long>& peList);

/* Unique instance number per process. */
		LONG instance_;
		static LONG volatile instance_count_;

/* Plugin Xml identifiers. */
		std::string plugin_id_, plugin_type_;

/* Application configuration. */
		config_t config_;

/* Significant failure has occurred, so ignore all runtime events flag. */
		bool is_shutdown_;

/* SNMP implant. */
		std::unique_ptr<snmp_agent_t> snmp_agent_;
		friend class snmp_agent_t;

#ifdef PSYCHMIB_H
		friend Netsnmp_Next_Data_Point psychPluginTable_get_next_data_point;
		friend Netsnmp_Node_Handler psychPluginTable_handler;

		friend Netsnmp_Next_Data_Point psychPluginPerformanceTable_get_next_data_point;
		friend Netsnmp_Node_Handler psychPluginPerformanceTable_handler;

		friend Netsnmp_First_Data_Point psychSessionTable_get_first_data_point;
		friend Netsnmp_Next_Data_Point psychSessionTable_get_next_data_point;

		friend Netsnmp_First_Data_Point psychSessionPerformanceTable_get_first_data_point;
		friend Netsnmp_Next_Data_Point psychSessionPerformanceTable_get_next_data_point;
#endif /* PSYCHMIB_H */

/* RFA context. */
		std::shared_ptr<rfa_t> rfa_;

/* RFA asynchronous event queue. */
		std::shared_ptr<rfa::common::EventQueue> event_queue_;

/* RFA logging */
		std::shared_ptr<logging::LogEventProvider> log_;

/* RFA provider */
		std::shared_ptr<provider_t> provider_;

/* Publish instruments. */
		std::map<resource_t, std::shared_ptr<connection_t>, resource_compare_t> connections_;
		std::map<resource_t, std::map<std::string, std::shared_ptr<broadcast_stream_t>>, resource_compare_t> stream_vector_;
		boost::shared_mutex query_mutex_;

/* Event pump and thread. */
		std::unique_ptr<event_pump_t> event_pump_;
		std::unique_ptr<boost::thread> thread_;

/* Publish fields. */
		rfa::data::FieldList fields_;

/* libcurl multi-interface context */
		ms::unique_handle<CURLM*, multipass_traits> multipass_;

/* libcurl thread safety */
		static LONG volatile curl_ref_count_;

/* Threadpool timer. */
		ms::timer timer_;

/** Performance Counters. **/
		boost::posix_time::ptime last_activity_;
		boost::posix_time::time_duration min_tcl_time_, max_tcl_time_, total_tcl_time_;
		boost::posix_time::time_duration min_refresh_time_, max_refresh_time_, total_refresh_time_;

		uint32_t cumulative_stats_[PSYCH_PC_MAX];
		uint32_t snap_stats_[PSYCH_PC_MAX];
		boost::posix_time::ptime snap_time_;
	};

} /* namespace psych */

#endif /* __PSYCH_HH__ */

/* eof */
