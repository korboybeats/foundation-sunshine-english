/**
 * @file tests/unit/test_webhook.cpp
 * @brief Test webhook functionality.
 */

#include <string>
#include <unordered_map>

#include "../tests_common.h"
#include "webhook.h"
#include "config.h"

using namespace std::literals;

struct WebhookTest : testing::Test {
  void SetUp() override {
    // Reset webhook config to defaults
    config::webhook.enabled = false;
    config::webhook.url = "";
    config::webhook.skip_ssl_verify = false;
    config::webhook.timeout = std::chrono::milliseconds(1000);
  }
};

TEST_F(WebhookTest, IsEnabledCheck) {
  // Test when disabled
  EXPECT_FALSE(webhook::is_enabled());
  
  // Test when enabled but no URL
  config::webhook.enabled = true;
  EXPECT_FALSE(webhook::is_enabled());
  
  // Test when enabled with URL
  config::webhook.url = "https://example.com/webhook";
  EXPECT_TRUE(webhook::is_enabled());
}

TEST_F(WebhookTest, AlertMessageLocalization) {
  // English-only build: messages are always English regardless of is_chinese flag
  for (bool is_chinese : { true, false }) {
    EXPECT_EQ(webhook::get_alert_message(webhook::event_type_t::CONFIG_PIN_SUCCESS, is_chinese), "🔗 Config pairing successful");
    EXPECT_EQ(webhook::get_alert_message(webhook::event_type_t::NV_APP_LAUNCH, is_chinese), "🚀 application launched");
    EXPECT_EQ(webhook::get_alert_message(webhook::event_type_t::NV_APP_RESUME, is_chinese), "▶️ application resumed");
  }
}

TEST_F(WebhookTest, JsonStringSanitization) {
  // Test basic escaping
  EXPECT_EQ(webhook::sanitize_json_string("Hello \"World\""), "Hello \\\"World\\\"");
  EXPECT_EQ(webhook::sanitize_json_string("Line1\nLine2"), "Line1\\nLine2");
  EXPECT_EQ(webhook::sanitize_json_string("Tab\tHere"), "Tab\\tHere");
  EXPECT_EQ(webhook::sanitize_json_string("Back\\slash"), "Back\\\\slash");
  
  // Test control characters
  EXPECT_EQ(webhook::sanitize_json_string("Text\x01\x02\x03"), "Text");
  
  // Test empty string
  EXPECT_EQ(webhook::sanitize_json_string(""), "");
  
  // Test special characters that should be preserved
  EXPECT_EQ(webhook::sanitize_json_string("Hello World"), "Hello World");
  EXPECT_EQ(webhook::sanitize_json_string("Emoji 🚀"), "Emoji 🚀");
}

TEST_F(WebhookTest, TimestampFormat) {
  std::string timestamp = webhook::get_current_timestamp();
  
  // Check format: YYYY-MM-DDTHH:MM:SS.sss
  EXPECT_EQ(timestamp.length(), 23);
  EXPECT_EQ(timestamp[4], '-');
  EXPECT_EQ(timestamp[7], '-');
  EXPECT_EQ(timestamp[10], 'T');
  EXPECT_EQ(timestamp[13], ':');
  EXPECT_EQ(timestamp[16], ':');
  EXPECT_EQ(timestamp[19], '.');
}

TEST_F(WebhookTest, EventStructure) {
  webhook::event_t event;
  event.type = webhook::event_type_t::NV_APP_LAUNCH;
  event.alert_type = "nv_app_launch";
  event.timestamp = "2024-01-01T12:00:00.000Z";
  event.client_name = "Test Client";
  event.client_ip = "192.168.1.100";
  event.app_name = "Test App";
  event.app_id = 123;
  event.session_id = "session123";
  event.extra_data = {{"resolution", "1920x1080"}, {"fps", "60"}};
  
  EXPECT_EQ(event.type, webhook::event_type_t::NV_APP_LAUNCH);
  EXPECT_EQ(event.alert_type, "nv_app_launch");
  EXPECT_EQ(event.client_name, "Test Client");
  EXPECT_EQ(event.app_id, 123);
  EXPECT_EQ(event.extra_data.size(), 2);
  EXPECT_EQ(event.extra_data["resolution"], "1920x1080");
  EXPECT_EQ(event.extra_data["fps"], "60");
}

// Note: tests for `webhook::generate_webhook_json` were removed because the
// function was declared but never defined. The English-only build relies on
// `webhook::g_webhook_format.generate_json_payload(...)` instead.

TEST_F(WebhookTest, RateLimiting) {
  // Test rate limiting functionality
  config::webhook.enabled = true;
  config::webhook.url = "http://example.com/webhook";
  
  // Initially should not be rate limited
  EXPECT_FALSE(webhook::is_rate_limited());
  
  // Record some successful sends (simulate)
  for (int i = 0; i < 10; ++i) {
    webhook::record_successful_send();
  }
  
  // Should now be rate limited
  EXPECT_TRUE(webhook::is_rate_limited());
}

TEST_F(WebhookTest, ThreadManagement) {
  // Test thread management functionality
  config::webhook.enabled = true;
  config::webhook.url = "http://example.com/webhook";
  
  // Initially should be able to create threads
  EXPECT_TRUE(webhook::can_create_thread());
  
  // Register some threads (simulate)
  for (int i = 0; i < 10; ++i) {
    webhook::register_thread();
  }
  
  // Should now be at thread limit
  EXPECT_FALSE(webhook::can_create_thread());
  
  // Unregister some threads
  for (int i = 0; i < 5; ++i) {
    webhook::unregister_thread();
  }
  
  // Should now be able to create threads again
  EXPECT_TRUE(webhook::can_create_thread());
}

TEST_F(WebhookTest, ThreadLimitEnforcement) {
  // Test that thread limit is actually enforced
  config::webhook.enabled = true;
  config::webhook.url = "http://example.com/webhook";
  
  // Create a simple event
  webhook::event_t event;
  event.type = webhook::event_type_t::CONFIG_PIN_SUCCESS;
  event.alert_type = "test";
  event.timestamp = "2024-01-01T12:00:00.000Z";
  
  // Fill up to thread limit
  for (int i = 0; i < 10; ++i) {
    webhook::register_thread();
  }
  
  // Now try to send an event - should be blocked by thread limit
  // This is a bit tricky to test since send_event_async is async,
  // but we can at least verify the can_create_thread check
  EXPECT_FALSE(webhook::can_create_thread());
  
  // Clean up
  for (int i = 0; i < 10; ++i) {
    webhook::unregister_thread();
  }
}

TEST_F(WebhookTest, ThreadRegistrationOrder) {
  // Test that thread registration happens before thread creation
  config::webhook.enabled = true;
  config::webhook.url = "http://example.com/webhook";
  
  // Initially should be able to create threads
  EXPECT_TRUE(webhook::can_create_thread());
  
  // Register 9 threads
  for (int i = 0; i < 9; ++i) {
    webhook::register_thread();
  }
  
  // Should still be able to create one more thread
  EXPECT_TRUE(webhook::can_create_thread());
  
  // Register the 10th thread
  webhook::register_thread();
  
  // Now should be at limit
  EXPECT_FALSE(webhook::can_create_thread());
  
  // Clean up
  for (int i = 0; i < 10; ++i) {
    webhook::unregister_thread();
  }
}

TEST_F(WebhookTest, RateLimitNotificationBypass) {
  // Test that rate limit notification bypasses thread limit
  config::webhook.enabled = true;
  config::webhook.url = "http://example.com/webhook";
  
  // Fill up to thread limit
  for (int i = 0; i < 10; ++i) {
    webhook::register_thread();
  }
  
  // Should be at thread limit
  EXPECT_FALSE(webhook::can_create_thread());
  
  // Rate limit notification should still be able to send
  // (This is tested by the fact that send_rate_limit_notification doesn't check can_create_thread)
  // We can't easily test the actual sending, but we can verify the function exists and doesn't crash
  webhook::send_rate_limit_notification();
  
  // Clean up
  for (int i = 0; i < 10; ++i) {
    webhook::unregister_thread();
  }
}
