// Application management constants
export const APP_CONSTANTS = {
  // Message types
  MESSAGE_TYPES: {
    SUCCESS: 'success',
    ERROR: 'error',
    WARNING: 'warning',
    INFO: 'info'
  },

  // Message icon mapping
  MESSAGE_ICONS: {
    success: 'fa-check-circle',
    error: 'fa-exclamation-circle',
    warning: 'fa-exclamation-triangle',
    info: 'fa-info-circle'
  },

  // Default app configuration
  DEFAULT_APP: {
    name: "",
    output: "",
    cmd: "",
    index: -1,
    "exclude-global-prep-cmd": false,
    elevated: false,
    "auto-detach": true,
    "wait-all": true,
    "exit-timeout": 5,
    "prep-cmd": [],
    "menu-cmd": [],
    detached: [],
    "image-path": "",
    "working-dir": ""
  },

  // Supported platforms
  PLATFORMS: {
    WINDOWS: 'windows',
    LINUX: 'linux',
    MACOS: 'macos'
  },

  // View modes
  VIEW_MODES: {
    GRID: 'grid',
    LIST: 'list'
  },

  // Message auto-hide time
  MESSAGE_AUTO_HIDE_TIME: 3000,

  // Drag animation duration
  DRAG_ANIMATION_TIME: 300,

  // Copy success animation duration
  COPY_SUCCESS_ANIMATION_TIME: 400,

  // Search debounce time
  SEARCH_DEBOUNCE_TIME: 300,

  // Text truncation length
  TEXT_TRUNCATE_LENGTH: 50
};

// Environment variable configuration
export const ENV_VARS_CONFIG = {
  'SUNSHINE_APP_ID': 'apps.env_app_id',
  'SUNSHINE_APP_NAME': 'apps.env_app_name',
  'SUNSHINE_CLIENT_NAME': 'apps.env_client_name',
  'SUNSHINE_CLIENT_WIDTH': 'apps.env_client_width',
  'SUNSHINE_CLIENT_HEIGHT': 'apps.env_client_height',
  'SUNSHINE_CLIENT_FPS': 'apps.env_client_fps',
  'SUNSHINE_CLIENT_HDR': 'apps.env_client_hdr',
  'SUNSHINE_CLIENT_GCMAP': 'apps.env_client_gcmap',
  'SUNSHINE_CLIENT_HOST_AUDIO': 'apps.env_client_host_audio',
  'SUNSHINE_CLIENT_ENABLE_SOPS': 'apps.env_client_enable_sops',
  'SUNSHINE_CLIENT_AUDIO_CONFIGURATION': 'apps.env_client_audio_config'
};

// API endpoints
export const API_ENDPOINTS = {
  APPS: '/api/apps',
  CONFIG: '/api/config',
  APP_DELETE: (index) => `/api/apps/${index}`
};