# Sunshine Webhook Format Configuration

## Overview

Sunshine supports several webhook formats — Markdown, plain text, and JSON. There is currently no adapter for any specific platform.

## Webhook Format Specification

### Message format requirements
- **msgtype**: Message type. Supports `markdown`, `text`, `json`.
- **content**: Message body. Maximum 4096 bytes; must be UTF-8 encoded.

### Supported Markdown syntax
- Standard Markdown syntax
- HTML tags (e.g. `<font color="warning">`)
- Block quotes (`>`)
- Bold (`**text**`)

## Configuration

### 1. Automatic configuration (default)
```cpp
// The system automatically configures Markdown format
configure_webhook_format(true);  // Use Markdown format
```

### 2. Manual configuration
```cpp
// Configure as Markdown (recommended)
g_webhook_format.set_format_type(format_type_t::MARKDOWN);
g_webhook_format.set_use_colors(true);
g_webhook_format.set_simplify_ip(true);

// Or configure as plain text
configure_webhook_format(false);
```

## Output Format Examples

### Pairing-failure notification
```json
{
    "msgtype": "markdown",
    "markdown": {
        "content": "**Sunshine system notification**\n\n<font color=\"warning\">**Pairing failed**</font>\n\n>Hostname:<font color=\"comment\">sunshine</font>\n>IP address:<font color=\"comment\">IPv6 (link-local)</font>\n>Client name:<font color=\"comment\">1111</font>\n>Time:<font color=\"comment\">2025-10-07 16:36:33</font>\n>Error:<font color=\"warning\">PIN verification failed</font>"
    }
}
```

### Application start notification
```json
{
    "msgtype": "markdown",
    "markdown": {
        "content": "**Sunshine system notification**\n\n<font color=\"info\">**Application started**</font>\n\n>Hostname:<font color=\"comment\">sunshine</font>\n>IP address:<font color=\"comment\">192.168.1.100</font>\n>App name:<font color=\"comment\">Steam</font>\n>App ID:<font color=\"comment\">12345</font>\n>Client:<font color=\"comment\">Moonlight</font>\n>Client IP:<font color=\"comment\">192.168.1.50</font>\n>Resolution:<font color=\"comment\">1920x1080</font>\n>Frame rate:<font color=\"comment\">60</font>\n>Audio:<font color=\"comment\">enabled</font>\n>Time:<font color=\"comment\">2025-10-07 16:36:33</font>"
    }
}
```

## Color Conventions

Sunshine webhooks support the following colors:
- `info` — informational (green)
- `warning` — warning (orange)
- `error` — error (red)
- `comment` — comment (gray)

## Content Length Limit

- Maximum length: 4096 bytes
- Auto-truncation: content exceeding the limit is truncated and an ellipsis is appended
- Logging: a warning is logged when truncation occurs

## Custom Templates

### Setting a custom template
```cpp
g_webhook_format.set_format_type(format_type_t::CUSTOM);
g_webhook_format.set_custom_template(
    event_type_t::CONFIG_PIN_FAILED,
    "🚨 **Pairing Failure Notification**\n\n"
    "**Host:** {{hostname}}\n"
    "**IP:** {{ip_address}}\n"
    "**Client:** {{client_name}}\n"
    "**Time:** {{timestamp}}\n"
    "**Error:** {{error}}"
);
```

### Supported template variables
- `{{hostname}}` — hostname
- `{{ip_address}}` — IP address
- `{{event_title}}` — event title
- `{{timestamp}}` — timestamp
- `{{client_name}}` — client name
- `{{client_ip}}` — client IP
- `{{app_name}}` — application name
- `{{app_id}}` — application ID
- `{{session_id}}` — session ID

## Validation Functions

### Checking content length
```cpp
std::string content = generate_content(event, is_chinese);
if (validate_webhook_content_length(content)) {
    // Content length is within the limit
} else {
    // Content is too long; truncation required
}
```

## Best Practices

1. **Use Markdown format** — provides a richer visual presentation
2. **Enable colors** — use color to distinguish event types
3. **Simplify IP display** — avoid showing verbose IPv6 addresses
4. **Control content length** — stay within the 4096-byte limit
5. **Use block quotes** — improves the visual hierarchy of information

## Troubleshooting

### Common issues
1. **Content too long** — check whether you exceeded the 4096-byte limit
2. **Format errors** — ensure the JSON is valid
3. **Encoding issues** — ensure UTF-8 encoding
4. **Colors not displayed** — check whether the receiver supports HTML tags

### Debugging
```cpp
// Enable debug logging
BOOST_LOG(debug) << "Webhook content: " << content;
BOOST_LOG(debug) << "Content length: " << content.length();
```
