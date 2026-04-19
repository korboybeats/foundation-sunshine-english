/**
 * @file src/webhook_format.h
 * @brief Webhook format configuration and template definitions
 */
#pragma once

#include <string>
#include <map>
#include <functional>
#include "webhook.h"

namespace webhook {

  /**
   * @brief Webhook format type
   */
  enum class format_type_t {
    MARKDOWN,     // Markdown format (supports HTML tags)
    TEXT,         // Plain text format
    JSON,         // JSON format
    CUSTOM        // Custom format
  };

  /**
   * @brief Color type definitions
   */
  namespace colors {
    constexpr const char* COLOR_INFO = "info";           // Info (green)
    constexpr const char* COLOR_WARNING = "warning";     // Warning (orange)
    constexpr const char* COLOR_ERROR = "error";         // Error (red)
    constexpr const char* COLOR_COMMENT = "comment";     // Comment (gray)
    constexpr const char* COLOR_SUCCESS = "success";     // Success (blue)
  }

  /**
   * @brief Webhook format configuration class
   */
  class WebhookFormat {
  public:
    /**
     * @brief Constructor
     * @param format_type Format type
     */
    explicit WebhookFormat(format_type_t format_type = format_type_t::MARKDOWN);

    /**
     * @brief Set the format type
     * @param format_type Format type
     */
    void set_format_type(format_type_t format_type);

    /**
     * @brief Get the format type
     * @return Format type
     */
    format_type_t get_format_type() const;

    /**
     * @brief Set a custom template
     * @param event_type Event type
     * @param template_str Template string
     */
    void set_custom_template(event_type_t event_type, const std::string& template_str);

    /**
     * @brief Set whether to use colors
     * @param use_colors Whether to use colors
     */
    void set_use_colors(bool use_colors);

    /**
     * @brief Set whether to simplify the IP display
     * @param simplify_ip Whether to simplify the IP display
     */
    void set_simplify_ip(bool simplify_ip);

    /**
     * @brief Set the time format
     * @param time_format Time format string
     */
    void set_time_format(const std::string& time_format);

    /**
     * @brief Generate the webhook content
     * @param event Event data
     * @param is_chinese Whether to use Chinese localization (ignored in English-only build)
     * @return Formatted content string
     */
    std::string generate_content(const event_t& event, bool is_chinese) const;

    /**
     * @brief Generate the full JSON payload
     * @param event Event data
     * @param is_chinese Whether to use Chinese localization (ignored in English-only build)
     * @return JSON string
     */
    std::string generate_json_payload(const event_t& event, bool is_chinese) const;

  private:
    format_type_t format_type_;
    bool use_colors_;
    bool simplify_ip_;
    std::string time_format_;
    std::map<event_type_t, std::string> custom_templates_;

    /**
     * @brief Format an IP address
     * @param ip IP address string
     * @return Formatted IP address
     */
    std::string format_ip_address(const std::string& ip) const;

    /**
     * @brief Format a timestamp
     * @param timestamp ISO 8601 formatted timestamp
     * @return Formatted time string
     */
    std::string format_timestamp(const std::string& timestamp) const;

    /**
     * @brief Get the color for an event
     * @param event_type Event type
     * @return Color string
     */
    std::string get_event_color(event_type_t event_type) const;

    /**
     * @brief Get the title for an event
     * @param event_type Event type
     * @param is_chinese Whether to use Chinese localization (ignored in English-only build)
     * @return Event title
     */
    std::string get_event_title(event_type_t event_type, bool is_chinese) const;

    /**
     * @brief Generate Markdown formatted content
     * @param event Event data
     * @param is_chinese Whether to use Chinese localization (ignored in English-only build)
     * @return Markdown content
     */
    std::string generate_markdown_content(const event_t& event, bool is_chinese) const;

    /**
     * @brief Generate text formatted content
     * @param event Event data
     * @param is_chinese Whether to use Chinese localization (ignored in English-only build)
     * @return Text content
     */
    std::string generate_text_content(const event_t& event, bool is_chinese) const;

    /**
     * @brief Generate JSON formatted content
     * @param event Event data
     * @param is_chinese Whether to use Chinese localization (ignored in English-only build)
     * @return JSON content
     */
    std::string generate_json_content(const event_t& event, bool is_chinese) const;

    /**
     * @brief Generate custom formatted content
     * @param event Event data
     * @param is_chinese Whether to use Chinese localization (ignored in English-only build)
     * @return Custom content
     */
    std::string generate_custom_content(const event_t& event, bool is_chinese) const;

    /**
     * @brief Substitute template variables
     * @param template_str Template string
     * @param event Event data
     * @param is_chinese Whether to use Chinese localization (ignored in English-only build)
     * @return Substituted string
     */
    std::string replace_template_variables(const std::string& template_str,
                                          const event_t& event,
                                          bool is_chinese) const;
  };

  /**
   * @brief Global webhook format instance
   */
  extern WebhookFormat g_webhook_format;

  /**
   * @brief Initialize the webhook format configuration
   */
  void init_webhook_format();

  /**
   * @brief Load format settings from the configuration file
   */
  void load_format_config();

  /**
   * @brief Configure the format
   * @param use_markdown Whether to use Markdown format (default: true)
   */
  void configure_webhook_format(bool use_markdown = true);

  /**
   * @brief Validate that the content length meets webhook requirements
   * @param content Content string
   * @return Whether the content meets the length requirement
   */
  bool validate_webhook_content_length(const std::string& content);

} // namespace webhook
