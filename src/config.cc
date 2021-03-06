/* User-configurable settings.
 */

#include "config.hh"

#include "chromium/logging.hh"
#include "chromium/values.hh"

psych::config_t::config_t() :
/* default values */
	is_snmp_enabled (false),
	is_agentx_subagent (true)
{
/* C++11 initializer lists not supported in MSVC2010 */
}

/* Minimal error handling parsing of an Xml node pulled from the
 * Analytics Engine.
 *
 * Returns true if configuration is valid, returns false on invalid content.
 */

using namespace xercesc;

/** L"" prefix is used in preference to u"" because of MSVC2010 **/

bool
psych::config_t::Validate()
{
	if (service_name.empty()) {
		LOG(ERROR) << "Undefined service name.";
		return false;
	}
	if (sessions.empty()) {
		LOG(ERROR) << "Undefined session, expecting one or more session node.";
		return false;
	}
	for (auto it = sessions.begin();
		it != sessions.end();
		++it)
	{
		if (it->session_name.empty()) {
			LOG(ERROR) << "Undefined session name.";
			return false;
		}
		if (it->connection_name.empty()) {
			LOG(ERROR) << "Undefined connection name for <session name=\"" << it->session_name << "\">.";
			return false;
		}
		if (it->publisher_name.empty()) {
			LOG(ERROR) << "Undefined publisher name for <session name=\"" << it->session_name << "\">.";
			return false;
		}
		if (it->rssl_servers.empty()) {
			LOG(ERROR) << "Undefined server list for <connection name=\"" << it->connection_name << "\">.";
			return false;
		}
		if (it->application_id.empty()) {
			LOG(ERROR) << "Undefined application ID for <session name=\"" << it->session_name << "\">.";
			return false;
		}
		if (it->instance_id.empty()) {
			LOG(ERROR) << "Undefined instance ID for <session name=\"" << it->session_name << "\">.";
			return false;
		}
		if (it->user_name.empty()) {
			LOG(ERROR) << "Undefined user name for <session name=\"" << it->session_name << "\">.";
			return false;
		}
	}
	if (monitor_name.empty()) {
		LOG(ERROR) << "Undefined monitor name.";
		return false;
	}
	if (event_queue_name.empty()) {
		LOG(ERROR) << "Undefined event queue name.";
		return false;
	}
	if (vendor_name.empty()) {
		LOG(ERROR) << "Undefined vendor name.";
		return false;
	}

/* Maximum data size & maximum response size must be provided for buffer allocation. */
	long value;

	if (maximum_data_size.empty()) {
		LOG(ERROR) << "Undefined maximum data size.";
		return false;
	}
	value = std::atol (maximum_data_size.c_str());
	if (value <= 0) {
		LOG(ERROR) << "Invalid maximum data size \"" << maximum_data_size << "\".";
		return false;
	}

	if (maximum_response_size.empty()) {
		LOG(ERROR) << "Undefined maximum response size.";
		return false;
	}
	value = std::atol (maximum_response_size.c_str());
	if (value <= 0) {
		LOG(ERROR) << "Invalid maximum response size \"" << maximum_response_size << "\".";
		return false;
	}

/* "resources" */
	for (auto it = resources.begin(); it != resources.end(); ++it) {
		if (it->name.empty()) {
			LOG(ERROR) << "Undefined resource name.";
			return false;
		}
		if (it->path.empty()) {
			LOG(ERROR) << "Undefined " << it->name << " feed path.";
			return false;
		}
		if (it->fields.empty()) {
			LOG(ERROR) << "Undefined " << it->name << " column FID mapping.";
			return false;
		}
		if (it->items.empty()) {
			LOG(ERROR) << "Undefined " << it->name << " sector: RIC and topic mapping.";
			return false;
		}
	}
	return true;
}

#ifndef CONFIG_PSYCH_AS_APPLICATION
/* Parse configuration from provided XML tree.
 */

bool
psych::config_t::ParseDomElement (
	const DOMElement*	root
	)
{
	vpf::XMLStringPool xml;
	const DOMNodeList* nodeList;

	LOG(INFO) << "Parsing configuration ...";
/* Plugin configuration wrapped within a <config> node. */
	nodeList = root->getElementsByTagName (L"config");

	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!ParseConfigNode (nodeList->item (i))) {
			LOG(ERROR) << "Failed parsing <config> nth-node #" << (1 + i) << '.';
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <config> nodes found in configuration.";

	if (!Validate()) {
		LOG(ERROR) << "Failed validation, malformed configuration file requires correction.";
		return false;
	}

	LOG(INFO) << "Parsing complete.";
	return true;
}

bool
psych::config_t::ParseConfigNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	const DOMNodeList* nodeList;

/* <Snmp> */
	nodeList = elem->getElementsByTagName (L"Snmp");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!ParseSnmpNode (nodeList->item (i))) {
			LOG(ERROR) << "Failed parsing <Snmp> nth-node #" << (1 + i) << '.';
			return false;
		}
	}
/* <Rfa> */
	nodeList = elem->getElementsByTagName (L"Rfa");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!ParseRfaNode (nodeList->item (i))) {
			LOG(ERROR) << "Failed parsing <Rfa> nth-node #" << (1 + i) << '.';
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <Rfa> nodes found in configuration.";
/* <psych> */
	nodeList = elem->getElementsByTagName (L"psych");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!ParsePsychNode (nodeList->item (i))) {
			LOG(ERROR) << "Failed parsing <psych> nth-node #" << (1 + i) << '.';
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <crosses> nodes found in configuration.";
	return true;
}

/* <Snmp> */
bool
psych::config_t::ParseSnmpNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	const DOMNodeList* nodeList;
	vpf::XMLStringPool xml;
	std::string attr;

/* logfile="file path" */
	attr = xml.transcode (elem->getAttribute (L"filelog"));
	if (!attr.empty())
		snmp_filelog = attr;

/* <agentX> */
	nodeList = elem->getElementsByTagName (L"agentX");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!ParseAgentXNode (nodeList->item (i))) {
			vpf::XMLStringPool xml;
			const std::string text_content = xml.transcode (nodeList->item (i)->getTextContent());
			LOG(ERROR) << "Failed parsing <agentX> nth-node #" << (1 + i) << ": \"" << text_content << "\".";
			return false;
		}
	}
	this->is_snmp_enabled = true;
	return true;
}

bool
psych::config_t::ParseAgentXNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* subagent="bool" */
	attr = xml.transcode (elem->getAttribute (L"subagent"));
	if (!attr.empty())
		is_agentx_subagent = (0 == attr.compare ("true"));

/* socket="..." */
	attr = xml.transcode (elem->getAttribute (L"socket"));
	if (!attr.empty())
		agentx_socket = attr;
	return true;
}

/* </Snmp> */

/* <Rfa> */
bool
psych::config_t::ParseRfaNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	const DOMNodeList* nodeList;
	std::string attr;

/* key="name" */
	attr = xml.transcode (elem->getAttribute (L"key"));
	if (!attr.empty())
		key = attr;

/* maximumDataSize="bytes" */
	attr = xml.transcode (elem->getAttribute (L"maximumDataSize"));
	if (!attr.empty())
		maximum_data_size = attr;

/* <service> */
	nodeList = elem->getElementsByTagName (L"service");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!ParseServiceNode (nodeList->item (i))) {
			const std::string text_content = xml.transcode (nodeList->item (i)->getTextContent());
			LOG(ERROR) << "Failed parsing <service> nth-node #" << (1 + i) << ": \"" << text_content << "\".";
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <service> nodes found in configuration.";
/* <DACS> */
	nodeList = elem->getElementsByTagName (L"DACS");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!ParseDacsNode (nodeList->item (i))) {
			const std::string text_content = xml.transcode (nodeList->item (i)->getTextContent());
			LOG(ERROR) << "Failed parsing <DACS> nth-node #" << (1 + i) << ": \"" << text_content << "\".";
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <DACS> nodes found in configuration.";
/* <session> */
	nodeList = elem->getElementsByTagName (L"session");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!ParseSessionNode (nodeList->item (i))) {
			LOG(ERROR) << "Failed parsing <session> nth-node #" << (1 + i) << ".";
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <session> nodes found, RFA behaviour is undefined without a server list.";
/* <monitor> */
	nodeList = elem->getElementsByTagName (L"monitor");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!ParseMonitorNode (nodeList->item (i))) {
			const std::string text_content = xml.transcode (nodeList->item (i)->getTextContent());
			LOG(ERROR) << "Failed parsing <monitor> nth-node #" << (1 + i) << ": \"" << text_content << "\".";
			return false;
		}
	}
/* <eventQueue> */
	nodeList = elem->getElementsByTagName (L"eventQueue");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!ParseEventQueueNode (nodeList->item (i))) {
			const std::string text_content = xml.transcode (nodeList->item (i)->getTextContent());
			LOG(ERROR) << "Failed parsing <eventQueue> nth-node #" << (1 + i) << ": \"" << text_content << "\".";
			return false;
		}
	}
/* <vendor> */
	nodeList = elem->getElementsByTagName (L"vendor");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!ParseVendorNode (nodeList->item (i))) {
			const std::string text_content = xml.transcode (nodeList->item (i)->getTextContent());
			LOG(ERROR) << "Failed parsing <vendor> nth-node #" << (1 + i) << ": \"" << text_content << "\".";
			return false;
		}
	}
	return true;
}

bool
psych::config_t::ParseServiceNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* name="name" */
	attr = xml.transcode (elem->getAttribute (L"name"));
	if (attr.empty()) {
/* service name cannot be empty */
		LOG(ERROR) << "Undefined \"name\" attribute, value cannot be empty.";
		return false;
	}
	service_name = attr;
	return true;
}

bool
psych::config_t::ParseDacsNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* id="numeric value" */
	attr = xml.transcode (elem->getAttribute (L"id"));
	if (!attr.empty())
		dacs_id = attr;
	LOG_IF(WARNING, dacs_id.empty()) << "Undefined DACS service ID.";
	return true;
}
bool
psych::config_t::ParseSessionNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	const DOMNodeList* nodeList;
	session_config_t session;

/* name="name" */
	session.session_name = xml.transcode (elem->getAttribute (L"name"));
	if (session.session_name.empty()) {
		LOG(ERROR) << "Undefined \"name\" attribute, value cannot be empty.";
		return false;
	}

/* <publisher> */
	nodeList = elem->getElementsByTagName (L"publisher");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!ParsePublisherNode (nodeList->item (i), session.publisher_name)) {
			const std::string text_content = xml.transcode (nodeList->item (i)->getTextContent());
			LOG(ERROR) << "Failed parsing <publisher> nth-node #" << (1 + i) << ": \"" << text_content << "\".";
			return false;
		}
	}
/* <connection> */
	nodeList = elem->getElementsByTagName (L"connection");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!ParseConnectionNode (nodeList->item (i), session)) {
			LOG(ERROR) << "Failed parsing <connection> nth-node #" << (1 + i) << '.';
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <connection> nodes found, RFA behaviour is undefined without a server list.";
/* <login> */
	nodeList = elem->getElementsByTagName (L"login");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!ParseLoginNode (nodeList->item (i), session)) {
			const std::string text_content = xml.transcode (nodeList->item (i)->getTextContent());
			LOG(ERROR) << "Failed parsing <login> nth-node #" << (1 + i) << ": \"" << text_content << "\".";
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <login> nodes found in configuration.";	
		
	sessions.push_back (session);
	return true;
}

bool
psych::config_t::ParseConnectionNode (
	const DOMNode*		node,
	session_config_t&	session
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	const DOMNodeList* nodeList;

/* name="name" */
	session.connection_name = xml.transcode (elem->getAttribute (L"name"));
	if (session.connection_name.empty()) {
		LOG(ERROR) << "Undefined \"name\" attribute, value cannot be empty.";
		return false;
	}
/* defaultPort="port" */
	session.rssl_default_port = xml.transcode (elem->getAttribute (L"defaultPort"));

/* <server> */
	nodeList = elem->getElementsByTagName (L"server");
	for (int i = 0; i < nodeList->getLength(); i++) {
		std::string server;
		if (!ParseServerNode (nodeList->item (i), server)) {
			const std::string text_content = xml.transcode (nodeList->item (i)->getTextContent());
			LOG(ERROR) << "Failed parsing <server> nth-node #" << (1 + i) << ": \"" << text_content << "\".";			
			return false;
		}
		session.rssl_servers.push_back (server);
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <server> nodes found, RFA behaviour is undefined without a server list.";

	return true;
}

bool
psych::config_t::ParseServerNode (
	const DOMNode*		node,
	std::string&		server
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	server = xml.transcode (elem->getTextContent());
	if (server.size() == 0) {
		LOG(ERROR) << "Undefined hostname or IPv4 address.";
		return false;
	}
	return true;
}

bool
psych::config_t::ParseLoginNode (
	const DOMNode*		node,
	session_config_t&	session
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;

/* applicationId="id" */
	session.application_id = xml.transcode (elem->getAttribute (L"applicationId"));
/* instanceId="id" */
	session.instance_id = xml.transcode (elem->getAttribute (L"instanceId"));
/* userName="name" */
	session.user_name = xml.transcode (elem->getAttribute (L"userName"));
/* position="..." */
	session.position = xml.transcode (elem->getAttribute (L"position"));
	return true;
}

bool
psych::config_t::ParseMonitorNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* name="name" */
	attr = xml.transcode (elem->getAttribute (L"name"));
	if (!attr.empty())
		monitor_name = attr;
	return true;
}

bool
psych::config_t::ParseEventQueueNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* name="name" */
	attr = xml.transcode (elem->getAttribute (L"name"));
	if (!attr.empty())
		event_queue_name = attr;
	return true;
}

bool
psych::config_t::ParsePublisherNode (
	const DOMNode*		node,
	std::string&		name
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;

/* name="name" */
	name = xml.transcode (elem->getAttribute (L"name"));
	return true;
}

bool
psych::config_t::ParseVendorNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* name="name" */
	attr = xml.transcode (elem->getAttribute (L"name"));
	if (!attr.empty())
		vendor_name = attr;
	return true;
}

/* </Rfa> */

/* <psych> */
bool
psych::config_t::ParsePsychNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	const DOMNodeList* nodeList;
	std::string attr;

/* interval="seconds" */
	attr = xml.transcode (elem->getAttribute (L"interval"));
	if (!attr.empty())
		interval = attr;
/* tolerableDelay="milliseconds" */
	attr = xml.transcode (elem->getAttribute (L"tolerableDelay"));
	if (!attr.empty())
		tolerable_delay = attr;
/* retryCount="count" */
	attr = xml.transcode (elem->getAttribute (L"retryCount"));
	if (!attr.empty())
		retry_count = attr;
/* retryDelayMs="milliseconds" */
	attr = xml.transcode (elem->getAttribute (L"retryDelayMs"));
	if (!attr.empty())
		retry_delay_ms = attr;
/* retryTimeoutMs="milliseconds" */
	attr = xml.transcode (elem->getAttribute (L"retryTimeoutMs"));
	if (!attr.empty())
		retry_timeout_ms = attr;
/* timeoutMs="milliseconds" */
	attr = xml.transcode (elem->getAttribute (L"timeoutMs"));
	if (!attr.empty())
		timeout_ms = attr;
/* connectTimeoutMs="milliseconds" */
	attr = xml.transcode (elem->getAttribute (L"connectTimeoutMs"));
	if (!attr.empty())
		connect_timeout_ms = attr;
/* enableHttpPipelining="boolean" */
	attr = xml.transcode (elem->getAttribute (L"enableHttpPipelining"));
	if (!attr.empty())
		enable_http_pipelining = attr;
/* maximumResponseSize="bytes" */
	attr = xml.transcode (elem->getAttribute (L"maximumResponseSize"));
	if (!attr.empty())
		maximum_response_size = attr;
/* minimumResponseSize="bytes" */
	attr = xml.transcode (elem->getAttribute (L"minimumResponseSize"));
	if (!attr.empty())
		minimum_response_size = attr;
/* requestHttpEncoding="encoding" */
	attr = xml.transcode (elem->getAttribute (L"requestHttpEncoding"));
	if (!attr.empty())
		request_http_encoding = attr;
/* timeOffsetConstant="time duration" */
	attr = xml.transcode (elem->getAttribute (L"timeOffsetConstant"));
	if (!attr.empty())
		time_offset_constant = attr;
/* panicThreshold="seconds" */
	attr = xml.transcode (elem->getAttribute (L"panicThreshold"));
	if (!attr.empty())
		panic_threshold = attr;
/* httpProxy="proxy" */
	attr = xml.transcode (elem->getAttribute (L"httpProxy"));
	if (!attr.empty())
		http_proxy = attr;
/* dnsCacheTimeout="seconds" */
	attr = xml.transcode (elem->getAttribute (L"dnsCacheTimeout"));
	if (!attr.empty())
		dns_cache_timeout = attr;
/* href="url" */
	attr = xml.transcode (elem->getAttribute (L"href"));
	if (!attr.empty())
		base_url = attr;

/* reset all lists */
	for (auto it = resources.begin(); it != resources.end(); ++it)
		it->fields.clear(), it->items.clear();
/* <resource> */
	nodeList = elem->getElementsByTagName (L"resource");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!ParseResourceNode (nodeList->item (i))) {
			LOG(ERROR) << "Failed parsing <resource> nth-node #" << (1 + i) << ".";
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <resource> nodes found.";
	return true;
}

bool
psych::config_t::ParseResourceNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	const DOMNodeList* nodeList;

	if (!elem->hasAttributes()) {
		LOG(ERROR) << "No attributes found, a \"name\" attribute is required.";
		return false;
	}
/* name="name" */
	std::string name (xml.transcode (elem->getAttribute (L"name")));
	if (name.empty()) {
		LOG(ERROR) << "Undefined \"name\" attribute.";
		return false;
	}

/* <field> */
	std::map<std::string, int> fields;
	nodeList = elem->getElementsByTagName (L"field");
	for (int i = 0; i < nodeList->getLength(); i++) {
		std::string field_name;
		int field_id;
		if (!ParseFieldNode (nodeList->item (i), &field_name, &field_id)) {
			LOG(ERROR) << "Failed parsing <field> nth-node #" << (1 + i) << ".";
			return false;
		}
		fields.emplace (std::make_pair (field_name, field_id));
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <field> nodes found.";
/* <item> */
	std::map<std::string, std::pair<std::string, std::string>> items;
	nodeList = elem->getElementsByTagName (L"item");
	for (int i = 0; i < nodeList->getLength(); i++) {
		std::string item_name, item_topic, item_src;
		if (!ParseItemNode (nodeList->item (i), &item_name, &item_topic, &item_src)) {
			LOG(ERROR) << "Failed parsing <item> nth-node #" << (1 + i) << ".";
			return false;
		}
		items.emplace (std::make_pair (item_src, std::make_pair (item_name, item_topic)));
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <item> nodes found.";

/* <link> */
	nodeList = elem->getElementsByTagName (L"link");
	for (int i = 0; i < nodeList->getLength(); i++) {
		std::string link_rel, link_name, link_href;
		unsigned long link_id;
		if (!ParseLinkNode (nodeList->item (i), &link_name, &link_rel, &link_id, &link_href)) {
			LOG(ERROR) << "Failed parsing <link> nth-node #" << (1 + i) << ".";
			return false;
		}
		resources.emplace_back (std::vector<resource_t>::value_type (name, link_name, link_href, link_id, fields, items));
	}
	if (0 == nodeList->getLength()) {
		LOG(WARNING) << "No <link> nodes found.";
		return true;
	}
	return true;
}

/* Parse <link rel="resource" id="entitlement code" href="URL"/>
 */

bool
psych::config_t::ParseLinkNode (
	const DOMNode*		node,
	std::string*		source,
	std::string*		rel,
	unsigned long*		id,
	std::string*		href
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;

	if (!elem->hasAttributes()) {
		LOG(ERROR) << "No attributes found, \"rel\", \"name\", and \"href\" attributes are required.";
		return false;
	}
/* rel="resource" */
	*rel = xml.transcode (elem->getAttribute (L"rel"));
	if (rel->empty()) {
		LOG(ERROR) << "Undefined \"rel\" attribute, value cannot be empty.";
		return false;
	}
/* name="source feed name" */
	*source = xml.transcode (elem->getAttribute (L"name"));
	if (source->empty()) {
		LOG(ERROR) << "Undefined \"name\" attribute, value cannot be empty.";
		return false;
	}
/* href="URL" */
	*href = xml.transcode (elem->getAttribute (L"href"));
	if (href->empty()) {
		LOG(ERROR) << "Undefined \"href\" attribute, value cannot be empty.";
		return false;
	}
/* id="integer" */
	const std::string id_text = xml.transcode (elem->getAttribute (L"id"));
	if (!id_text.empty())
		*id = std::strtoul (id_text.c_str(), nullptr, 10);
	else
		*id = 0;
	return true;
}

/* Parse <field name="name" id="id"/>
 */

bool
psych::config_t::ParseFieldNode (
	const DOMNode*		node,
	std::string*		name,
	int*			id
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;

	if (!elem->hasAttributes()) {
		LOG(ERROR) << "No attributes found, \"name\" and \"id\" attributes are required.";
		return false;
	}
/* name="text" */
	*name = xml.transcode (elem->getAttribute (L"name"));
	if (name->empty()) {
		LOG(ERROR) << "Undefined \"name\" attribute, value cannot be empty.";
		return false;
	}
/* id="integer" */
	const std::string id_text = xml.transcode (elem->getAttribute (L"id"));
	if (id_text.empty()) {
		LOG(ERROR) << "Undefined \"id\" attribute, value cannot be empty.";
		return false;
	}

	*id = std::stoi (id_text);
	return true;
}

/* Parse <item name="name" topic="topic" src="text"/>
 */

bool
psych::config_t::ParseItemNode (
	const DOMNode*		node,
	std::string*		name,
	std::string*		topic,
	std::string*		src
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;

	if (!elem->hasAttributes()) {
		LOG(ERROR) << "No attributes found, \"name\", \"topic\", and \"src\" attributes are required.";
		return false;
	}
/* name="text" */
	*name = xml.transcode (elem->getAttribute (L"name"));
	if (name->empty()) {
		LOG(ERROR) << "Undefined \"name\" attribute, value cannot be empty.";
		return false;
	}
/* topic="text" */
	*topic = xml.transcode (elem->getAttribute (L"topic"));
	if (topic->empty()) {
		LOG(ERROR) << "Undefined \"topic\" attribute, value cannot be empty.";
		return false;
	}
/* src="text" */
	*src = xml.transcode (elem->getAttribute (L"src"));
	if (src->empty()) {
		LOG(ERROR) << "Undefined \"src\" attribute, value cannot be empty.";
		return false;
	}
	return true;
}

/* </psych> */
/* </config> */

#endif /* CONFIG_PSYCH_AS_APPLICATION */

/* Parse configuration from provided JSON tree.
 */

bool
psych::config_t::ParseConfig (
	const chromium::DictionaryValue* dict_val
	)
{
	DCHECK(nullptr != dict_val);

	LOG(INFO) << "Parsing configuration ...";

/* validate */
	if (!dict_val->HasKey ("sessions")) {
		LOG(ERROR) << "Undefined \"sessions\" array.";
		return false;
	}
	if (!dict_val->HasKey ("resources")) {
		LOG(ERROR) << "Undefined \"resources\" array.";
		return false;
	}

/* Snmp */
	dict_val->GetBoolean ("is_snmp_enabled", &is_snmp_enabled);
	dict_val->GetBoolean ("is_agentx_subagent", &is_agentx_subagent);
	dict_val->GetString ("agentx_socket", &agentx_socket);

/* Configuration override */
	dict_val->GetString ("key", &key);

/* Rfa */
	dict_val->GetString ("service_name", &service_name);
	dict_val->GetString ("dacs_id", &dacs_id);
/* enumerate sessions */
	chromium::ListValue* list = nullptr;
	CHECK (dict_val->GetList ("sessions", &list));
	LOG_IF(WARNING, 0 == list->GetSize()) << "Empty \"sessions\" array.";
	for (unsigned i = 0; i < list->GetSize(); ++i) {
		chromium::Value* tmp_value;
		CHECK (list->Get (i, &tmp_value));
		CHECK (tmp_value->IsType (chromium::Value::TYPE_DICTIONARY));
		if (!ParseSession (static_cast<chromium::DictionaryValue*>(tmp_value)))
			return false;
	}
	dict_val->GetString ("monitor_name", &monitor_name);
	dict_val->GetString ("event_queue_name", &event_queue_name);
	dict_val->GetString ("vendor_name", &vendor_name);
	dict_val->GetString ("maximum_data_size", &maximum_data_size);

/* psych application parameters */
	dict_val->GetString ("interval", &interval);
	dict_val->GetString ("tolerable_delay", &tolerable_delay);
	dict_val->GetString ("retry_count", &retry_count);
	dict_val->GetString ("retry_delay_ms", &retry_delay_ms);
	dict_val->GetString ("retry_timeout_ms", &retry_timeout_ms);
	dict_val->GetString ("timeout_ms", &timeout_ms);
	dict_val->GetString ("connect_timeout_ms", &connect_timeout_ms);
	dict_val->GetString ("enable_http_pipelining", &enable_http_pipelining);
	dict_val->GetString ("maximum_response_size", &maximum_response_size);
	dict_val->GetString ("minimum_response_size", &minimum_response_size);
	dict_val->GetString ("request_http_encoding", &request_http_encoding);
	dict_val->GetString ("time_offset_constant", &time_offset_constant);
	dict_val->GetString ("panic_threshold", &panic_threshold);
	dict_val->GetString ("http_proxy", &http_proxy);
	dict_val->GetString ("dns_cache_timeout", &dns_cache_timeout);
	dict_val->GetString ("base_url", &base_url);
/* enumerate resources */
	CHECK (dict_val->GetList ("resources", &list));
	LOG_IF(WARNING, 0 == list->GetSize()) << "Empty \"resources\" array.";
	for (unsigned i = 0; i < list->GetSize(); ++i) {
		chromium::Value* tmp_value;
		CHECK (list->Get (i, &tmp_value));
		CHECK (tmp_value->IsType (chromium::Value::TYPE_DICTIONARY));
		if (!ParseResource (static_cast<chromium::DictionaryValue*>(tmp_value)))
			return false;
	}

	if (!Validate()) {
		LOG(ERROR) << "Failed validation, malformed configuration file requires correction.";
		return false;
	}

	LOG(INFO) << "Parsing complete.";
	return true;
}

bool
psych::config_t::ParseSession (
	const chromium::DictionaryValue* dict_val
	)
{
	DCHECK(nullptr != dict_val);

/* new session */
	session_config_t session;

	dict_val->GetString ("session_name", &session.session_name);
	dict_val->GetString ("connection_name", &session.connection_name);
	dict_val->GetString ("publisher_name", &session.publisher_name);
/* enumerate Rssl servers */
	chromium::ListValue* list = nullptr;
	CHECK (dict_val->GetList ("rssl_servers", &list));
	LOG_IF(WARNING, 0 == list->GetSize()) << "Empty \"rssl_servers\" array.";
	for (unsigned i = 0; i < list->GetSize(); ++i) {
		std::string rssl_server;
		CHECK (list->GetString (i, &rssl_server));
		session.rssl_servers.push_back (rssl_server);
	}
	dict_val->GetString ("rssl_default_port", &session.rssl_default_port);
	dict_val->GetString ("application_id", &session.application_id);
	dict_val->GetString ("instance_id", &session.instance_id);
	dict_val->GetString ("user_name", &session.user_name);
	dict_val->GetString ("position", &session.position);
	sessions.push_back (session);
	return true;
}

bool
psych::config_t::ParseResource (
	const chromium::DictionaryValue* dict_val
	)
{
	DCHECK(nullptr != dict_val);

	std::string name, source, path;
	unsigned long entitlement_code;
	std::map<std::string, int> fields;
	std::map<std::string, std::pair<std::string, std::string>> items;

	dict_val->GetString ("name", &name);
	dict_val->GetString ("source", &source);
	dict_val->GetString ("path", &path);
	int integer_value = 0;
	dict_val->GetInteger ("entitlement_code", &integer_value);
/* long promotion, lose any sign. */
	entitlement_code = integer_value;
/* enumerate fields */
	chromium::Value* tmp_value;
	CHECK (dict_val->Get ("fields", &tmp_value));
	CHECK (tmp_value->IsType (chromium::Value::TYPE_DICTIONARY));
	for (chromium::DictionaryValue::Iterator it (*static_cast<chromium::DictionaryValue*>(tmp_value));
		it.HasNext();
		it.Advance())
	{
		CHECK (it.value().IsType (chromium::Value::TYPE_INTEGER));
		CHECK (it.value().GetAsInteger (&integer_value));
		fields.emplace (std::make_pair (it.key(), integer_value));
	}
/* enumerate items */
	CHECK (dict_val->Get ("items", &tmp_value));
	CHECK (tmp_value->IsType (chromium::Value::TYPE_DICTIONARY));
	for (chromium::DictionaryValue::Iterator it (*static_cast<chromium::DictionaryValue*>(tmp_value));
		it.HasNext();
		it.Advance())
	{
		CHECK (it.value().IsType (chromium::Value::TYPE_DICTIONARY));
		const chromium::DictionaryValue& item_val = *static_cast<const chromium::DictionaryValue*>(&(it.value()));
		std::string item_name, item_topic;
		item_val.GetString ("RIC", &item_name);
		item_val.GetString ("topic", &item_topic);
		items.emplace (std::make_pair (it.key(), std::make_pair (item_name, item_topic)));
	}
/* new resource */
	resources.emplace_back (std::vector<resource_t>::value_type (name, source, path, entitlement_code, fields, items));
	return true;
}

/* eof */
