/**
 * @file src/webhook_format.cpp
 * @brief Webhook format configuration and template implementation
 */
#include "webhook_format.h"
#include "webhook.h"
#include "config.h"
#include "logging.h"
#include "platform/common.h"
#include <sstream>
#include <regex>

namespace webhook {

  // Global webhook format instance
  WebhookFormat g_webhook_format;

  WebhookFormat::WebhookFormat(format_type_t format_type)
    : format_type_(format_type)
    , use_colors_(true)
    , simplify_ip_(true)
    , time_format_("%Y-%m-%d %H:%M:%S")
  {
  }

  void WebhookFormat::set_format_type(format_type_t format_type) {
    format_type_ = format_type;
  }

  format_type_t WebhookFormat::get_format_type() const {
    return format_type_;
  }

  void WebhookFormat::set_custom_template(event_type_t event_type, const std::string& template_str) {
    custom_templates_[event_type] = template_str;
  }

  void WebhookFormat::set_use_colors(bool use_colors) {
    use_colors_ = use_colors;
  }

  void WebhookFormat::set_simplify_ip(bool simplify_ip) {
    simplify_ip_ = simplify_ip;
  }

  void WebhookFormat::set_time_format(const std::string& time_format) {
    time_format_ = time_format;
  }

  std::string WebhookFormat::format_ip_address(const std::string& ip) const {
    if (ip.empty()) return "";
    if (!simplify_ip_) {
      return ip;
    }
    // Handle IPv6 addresses
    if (ip.find(':') != std::string::npos) {
      // Simplify IPv6 display
      if (ip.find("fe80::") == 0) {
        return "IPv6 (link-local)";
      } else if (ip.find("::1") != std::string::npos) {
        return "IPv6 (loopback)";
      } else {
        return "IPv6";
      }
    }

    // Return IPv4 address as-is
    return ip;
  }

  std::string WebhookFormat::format_timestamp(const std::string& timestamp) const
  {
    // Convert ISO 8601 format to a more friendly format
    // 2025-10-07T16:36:33.595 -> 2025-10-07 16:36:33
    std::string formatted = timestamp;
    size_t dot_pos = formatted.find('.');
    if (dot_pos != std::string::npos) {
      formatted = formatted.substr(0, dot_pos);
    }
    size_t t_pos = formatted.find('T');
    if (t_pos != std::string::npos) {
      formatted[t_pos] = ' ';
    }
    return formatted;
  }

  std::string WebhookFormat::get_event_color(event_type_t event_type) const
  {
    if (!use_colors_) {
      return "";
    }

    switch (event_type) {
      case event_type_t::CONFIG_PIN_SUCCESS:
      case event_type_t::NV_APP_LAUNCH:
      case event_type_t::NV_APP_RESUME:
      case event_type_t::NV_SESSION_START:
        return colors::COLOR_INFO;

      case event_type_t::CONFIG_PIN_FAILED:
      case event_type_t::NV_APP_TERMINATE:
        return colors::COLOR_WARNING;

      case event_type_t::NV_SESSION_END:
        return colors::COLOR_COMMENT;

      default:
        return colors::COLOR_COMMENT;
    }
  }

  std::string WebhookFormat::get_event_title(event_type_t event_type, bool is_chinese) const
  {
    // English-only build: ignore is_chinese flag.
    (void) is_chinese;
    switch (event_type) {
      case event_type_t::CONFIG_PIN_SUCCESS:
        return "Config Pairing Successful";
      case event_type_t::CONFIG_PIN_FAILED:
        return "Config Pairing Failed";
      case event_type_t::NV_APP_LAUNCH:
        return "Application Launched";
      case event_type_t::NV_APP_RESUME:
        return "Application Resumed";
      case event_type_t::NV_APP_TERMINATE:
        return "Application Terminated";
      case event_type_t::NV_SESSION_START:
        return "Session Started";
      case event_type_t::NV_SESSION_END:
        return "Session Ended";
      default:
        return "System Notification";
    }
  }

  std::string WebhookFormat::generate_markdown_content(const event_t& event, bool is_chinese) const
  {
    // English-only build: ignore is_chinese flag.
    (void) is_chinese;
    std::ostringstream content_stream;
    // Get host info
    std::string hostname = platf::get_host_name();
    std::string local_ip = get_local_ip();
    std::string formatted_ip = format_ip_address(local_ip);
    content_stream << "**Sunshine System Notification**" << "\n\n";

    // Set color and content based on event type
    std::string event_title = get_event_title(event.type, false);
    std::string event_color = get_event_color(event.type);

    if (use_colors_ && !event_color.empty()) {
      content_stream << "<font color=\"" << event_color << "\">**" << event_title << "**</font>\n\n";
    } else {
      content_stream << "**" << event_title << "**\n\n";
    }
    // Add basic info
    content_stream << ">Hostname:<font color=\"comment\">" << hostname << "</font>\n";
    if (!formatted_ip.empty()) {
      content_stream << ">IP Address:<font color=\"comment\">" << formatted_ip << "</font>\n";
    }
    // Add event-specific info
    switch (event.type) {
      case event_type_t::CONFIG_PIN_SUCCESS:
      case event_type_t::CONFIG_PIN_FAILED: {
        if (!event.client_name.empty()) {
          content_stream << ">Client Name:<font color=\"comment\">" << event.client_name << "</font>\n";
        }
        if (!event.client_ip.empty()) {
          content_stream << ">Client IP:<font color=\"comment\">" << event.client_ip << "</font>\n";
        }
        if (!event.server_ip.empty()) {
          content_stream << ">Server IP:<font color=\"comment\">" << event.server_ip << "</font>\n";
        }
        break;
      }
      case event_type_t::NV_APP_LAUNCH:
      case event_type_t::NV_APP_RESUME:
      case event_type_t::NV_APP_TERMINATE: {
        if (!event.app_name.empty()) {
          content_stream << ">App Name:<font color=\"comment\">" << event.app_name << "</font>\n";
        }
        if (event.app_id > 0) {
          content_stream << ">App ID:<font color=\"comment\">" << event.app_id << "</font>\n";
        }
        if (!event.client_name.empty()) {
          content_stream << ">Client:<font color=\"comment\">" << event.client_name << "</font>\n";
        }
        if (!event.client_ip.empty()) {
          content_stream << ">Client IP:<font color=\"comment\">" << event.client_ip << "</font>\n";
        }
        if (!event.server_ip.empty()) {
          content_stream << ">Server IP:<font color=\"comment\">" << event.server_ip << "</font>\n";
        }
        // Add extra info
        for (const auto& [key, value] : event.extra_data) {
          if (key == "resolution") {
            content_stream << ">Resolution:<font color=\"comment\">" << value << "</font>\n";
          } else if (key == "fps") {
            content_stream << ">FPS:<font color=\"comment\">" << value << "</font>\n";
          } else if (key == "host_audio") {
            content_stream << ">Audio:<font color=\"comment\">"
                          << (value == "true" ? "Enabled" : "Disabled") << "</font>\n";
          }
        }
        break;
      }
      case event_type_t::NV_SESSION_START:
      case event_type_t::NV_SESSION_END: {
        if (!event.app_name.empty()) {
          content_stream << ">App Name:<font color=\"comment\">" << event.app_name << "</font>\n";
        }
        if (!event.client_name.empty()) {
          content_stream << ">Client:<font color=\"comment\">" << event.client_name << "</font>\n";
        }
        if (!event.session_id.empty()) {
          content_stream << ">Session ID:<font color=\"comment\">" << event.session_id << "</font>\n";
        }
        break;
      }
      default:
        break;
    }
    content_stream << ">Time:<font color=\"comment\">" << format_timestamp(event.timestamp) << "</font>";
    // Add error info
    auto error_it = event.extra_data.find("error");
    if (error_it != event.extra_data.end()) {
      content_stream << "\n>Error:<font color=\"warning\">" << error_it->second << "</font>";
    }
    return content_stream.str();
  }

  std::string WebhookFormat::generate_text_content(const event_t& event, bool is_chinese) const {
    // English-only build: ignore is_chinese flag.
    (void) is_chinese;
    std::ostringstream content_stream;

    std::string hostname = platf::get_host_name();
    std::string local_ip = get_local_ip();
    std::string formatted_ip = format_ip_address(local_ip);
    // Build plain text content
    content_stream << "Sunshine System Notification" << "\n";
    content_stream << "================================\n";
    content_stream << "Event: " << get_event_title(event.type, false) << "\n";
    content_stream << "Hostname: " << hostname << "\n";

    if (!formatted_ip.empty()) {
      content_stream << "IP Address: " << formatted_ip << "\n";
    }
    // Add event-specific info
    switch (event.type) {
      case event_type_t::CONFIG_PIN_SUCCESS:
      case event_type_t::CONFIG_PIN_FAILED: {
        if (!event.client_name.empty()) {
          content_stream << "Client Name: " << event.client_name << "\n";
        }
        if (!event.client_ip.empty()) {
          content_stream << "Client IP: " << event.client_ip << "\n";
        }
        if (!event.server_ip.empty()) {
          content_stream << "Server IP: " << event.server_ip << "\n";
        }
        break;
      }
      case event_type_t::NV_APP_LAUNCH:
      case event_type_t::NV_APP_RESUME:
      case event_type_t::NV_APP_TERMINATE: {
        if (!event.app_name.empty()) {
          content_stream << "App Name: " << event.app_name << "\n";
        }
        if (event.app_id > 0) {
          content_stream << "App ID: " << event.app_id << "\n";
        }
        if (!event.client_name.empty()) {
          content_stream << "Client: " << event.client_name << "\n";
        }
        if (!event.client_ip.empty()) {
          content_stream << "Client IP: " << event.client_ip << "\n";
        }
        if (!event.server_ip.empty()) {
          content_stream << "Server IP: " << event.server_ip << "\n";
        }
        break;
      }
      case event_type_t::NV_SESSION_START:
      case event_type_t::NV_SESSION_END: {
        if (!event.app_name.empty()) {
          content_stream << "App Name: " << event.app_name << "\n";
        }
        if (!event.client_name.empty()) {
          content_stream << "Client: " << event.client_name << "\n";
        }
        if (!event.session_id.empty()) {
          content_stream << "Session ID: " << event.session_id << "\n";
        }
        break;
      }
      default:
        break;
    }
    content_stream << "Time: " << format_timestamp(event.timestamp) << "\n";
    // Add error info
    auto error_it = event.extra_data.find("error");
    if (error_it != event.extra_data.end()) {
      content_stream << "Error: " << error_it->second << "\n";
    }
    return content_stream.str();
  }

  std::string WebhookFormat::generate_json_content(const event_t& event, bool is_chinese) const
  {
    // English-only build: ignore is_chinese flag.
    (void) is_chinese;
    std::ostringstream json_stream;
    std::string hostname = platf::get_host_name();
    std::string local_ip = get_local_ip();
    std::string formatted_ip = format_ip_address(local_ip);
    json_stream << "{";
    json_stream << "\"system\":\"Sunshine\",";
    json_stream << "\"hostname\":\"" << hostname << "\",";
    if (!formatted_ip.empty()) {
      json_stream << "\"ip_address\":\"" << formatted_ip << "\",";
    }
    json_stream << "\"event_type\":\"" << get_event_title(event.type, false) << "\",";
    json_stream << "\"timestamp\":\"" << format_timestamp(event.timestamp) << "\"";

    // Add event-specific fields
    if (!event.client_name.empty()) {
      json_stream << ",\"client_name\":\"" << event.client_name << "\"";
    }
    if (!event.client_ip.empty()) {
      json_stream << ",\"client_ip\":\"" << event.client_ip << "\"";
    }
    if (!event.server_ip.empty()) {
      json_stream << ",\"server_ip\":\"" << event.server_ip << "\"";
    }
    if (!event.app_name.empty()) {
      json_stream << ",\"app_name\":\"" << event.app_name << "\"";
    }
    if (event.app_id > 0) {
      json_stream << ",\"app_id\":" << event.app_id;
    }
    if (!event.session_id.empty()) {
      json_stream << ",\"session_id\":\"" << event.session_id << "\"";
    }

    // Add extra data
    if (!event.extra_data.empty()) {
      json_stream << ",\"extra_data\":{";
      bool first = true;
      for (const auto& [key, value] : event.extra_data) {
        if (!first) json_stream << ",";
        json_stream << "\"" << key << "\":\"" << value << "\"";
        first = false;
      }
      json_stream << "}";
    }

    json_stream << "}";
    return json_stream.str();
  }

  std::string WebhookFormat::generate_custom_content(const event_t& event, bool is_chinese) const
  {
    auto it = custom_templates_.find(event.type);
    if (it != custom_templates_.end()) {
      return replace_template_variables(it->second, event, is_chinese);
    }

    // If no custom template is defined, fall back to Markdown format
    return generate_markdown_content(event, is_chinese);
  }

  std::string WebhookFormat::replace_template_variables(const std::string& template_str, const event_t& event, bool is_chinese) const
  {
    std::string result = template_str;

    // Variable substitution
    std::string hostname = platf::get_host_name();
    std::string local_ip = get_local_ip();
    std::string formatted_ip = format_ip_address(local_ip);

    // Use regex to substitute variables
    result = std::regex_replace(result, std::regex("\\{\\{hostname\\}\\}"), hostname);
    result = std::regex_replace(result, std::regex("\\{\\{ip_address\\}\\}"), formatted_ip);
    result = std::regex_replace(result, std::regex("\\{\\{event_title\\}\\}"), get_event_title(event.type, is_chinese));
    result = std::regex_replace(result, std::regex("\\{\\{timestamp\\}\\}"), format_timestamp(event.timestamp));
    result = std::regex_replace(result, std::regex("\\{\\{client_name\\}\\}"), event.client_name);
    result = std::regex_replace(result, std::regex("\\{\\{client_ip\\}\\}"), event.client_ip);
    result = std::regex_replace(result, std::regex("\\{\\{server_ip\\}\\}"), event.server_ip);
    result = std::regex_replace(result, std::regex("\\{\\{app_name\\}\\}"), event.app_name);
    result = std::regex_replace(result, std::regex("\\{\\{app_id\\}\\}"), std::to_string(event.app_id));
    result = std::regex_replace(result, std::regex("\\{\\{session_id\\}\\}"), event.session_id);

    return result;
  }

  std::string WebhookFormat::generate_content(const event_t& event, bool is_chinese) const
  {
    switch (format_type_) {
      case format_type_t::MARKDOWN:
        return generate_markdown_content(event, is_chinese);
      case format_type_t::TEXT:
        return generate_text_content(event, is_chinese);
      case format_type_t::JSON:
        return generate_json_content(event, is_chinese);
      case format_type_t::CUSTOM:
        return generate_custom_content(event, is_chinese);
      default:
        return generate_markdown_content(event, is_chinese);
    }
  }

  std::string WebhookFormat::generate_json_payload(const event_t& event, bool is_chinese) const
  {
    std::string content = generate_content(event, is_chinese);

    // Enforce content length limit (4096 bytes)
    const size_t MAX_CONTENT_LENGTH = 4096;
    if (content.length() > MAX_CONTENT_LENGTH) {
      // Truncate content and append ellipsis
      content = content.substr(0, MAX_CONTENT_LENGTH - 10) + "...";
      BOOST_LOG(warning) << "Webhook content truncated to " << MAX_CONTENT_LENGTH << " bytes";
    }

    switch (format_type_) {
      case format_type_t::MARKDOWN:
        return "{\"msgtype\":\"markdown\",\"markdown\":{\"content\":\"" + sanitize_json_string(content) + "\"}}";
      case format_type_t::TEXT:
        return "{\"msgtype\":\"text\",\"text\":{\"content\":\"" + sanitize_json_string(content) + "\"}}";
      case format_type_t::JSON:
        return content; // JSON format returns content directly
      case format_type_t::CUSTOM:
        return "{\"msgtype\":\"markdown\",\"markdown\":{\"content\":\"" + sanitize_json_string(content) + "\"}}";
      default:
        return "{\"msgtype\":\"markdown\",\"markdown\":{\"content\":\"" + sanitize_json_string(content) + "\"}}";
    }
  }

  void init_webhook_format()
  {
    // Initialize default format configuration
    g_webhook_format.set_format_type(format_type_t::MARKDOWN);
    g_webhook_format.set_use_colors(true);
    g_webhook_format.set_simplify_ip(true);
    g_webhook_format.set_time_format("%Y-%m-%d %H:%M:%S");
  }

  void load_format_config()
  {
    // Load format settings from the configuration file
    // Logic to read format settings from config::webhook can be added here
    init_webhook_format();
  }

  void configure_webhook_format(bool use_markdown)
  {
    if (use_markdown) {
      g_webhook_format.set_format_type(format_type_t::MARKDOWN);
    } else {
      g_webhook_format.set_format_type(format_type_t::TEXT);
    }

    // Webhook optimization settings
    g_webhook_format.set_use_colors(true);      // Enable color support
    g_webhook_format.set_simplify_ip(true);     // Simplify IP display
    g_webhook_format.set_time_format("%Y-%m-%d %H:%M:%S"); // Standard time format

    BOOST_LOG(debug) << "Webhook configured (Markdown: " << use_markdown << ")";
  }

  bool validate_webhook_content_length(const std::string& content) {
    const size_t MAX_CONTENT_LENGTH = 4096;
    return content.length() <= MAX_CONTENT_LENGTH;
  }

} // namespace webhook
