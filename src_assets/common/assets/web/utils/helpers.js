/**
 * Debounce function
 * @param {Function} func Function to debounce
 * @param {number} wait Wait time in milliseconds
 * @returns {Function} Debounced function
 */
export function debounce(func, wait) {
  let timeout;
  return function executedFunction(...args) {
    const later = () => {
      clearTimeout(timeout);
      func(...args);
    };
    clearTimeout(timeout);
    timeout = setTimeout(later, wait);
  };
}

/**
 * Async delay helper
 * @param {number} ms Delay in milliseconds
 * @returns {Promise} Promise object
 */
export function delay(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

/**
 * Deep clone helper
 * @param {*} obj Object to deep clone
 * @returns {*} Deep-cloned object
 */
export function deepClone(obj) {
  if (obj === null || typeof obj !== 'object') return obj;
  if (obj instanceof Date) return new Date(obj);
  if (obj instanceof Array) return obj.map(item => deepClone(item));
  if (typeof obj === 'object') {
    const clonedObj = {};
    for (const key in obj) {
      if (obj.hasOwnProperty(key)) {
        clonedObj[key] = deepClone(obj[key]);
      }
    }
    return clonedObj;
  }
}

/**
 * Safe JSON parse
 * @param {string} str JSON string
 * @param {*} defaultValue Default value when parsing fails
 * @returns {*} Parsed result or default value
 */
export function safeJsonParse(str, defaultValue = null) {
  try {
    return JSON.parse(str);
  } catch (error) {
    console.warn('JSON parse failed:', error);
    return defaultValue;
  }
}

/**
 * Format error message
 * @param {Error|string} error Error object or message
 * @returns {string} Formatted error message
 */
export function formatError(error) {
  if (typeof error === 'string') return error;
  if (error && error.message) return error.message;
  return 'Unknown error';
}

/**
 * Check whether the value is a valid URL
 * @param {string} url URL string
 * @returns {boolean} Whether it is a valid URL
 */
export function isValidUrl(url) {
  try {
    new URL(url);
    return true;
  } catch {
    return false;
  }
}

/**
 * Get file extension
 * @param {string} filename File name
 * @returns {string} File extension
 */
export function getFileExtension(filename) {
  return filename.split('.').pop().toLowerCase();
}

/**
 * Format file size
 * @param {number} bytes Byte count
 * @param {number} decimals Decimal places
 * @returns {string} Formatted size
 */
export function formatFileSize(bytes, decimals = 2) {
  if (bytes === 0) return '0 Bytes';
  const k = 1024;
  const dm = decimals < 0 ? 0 : decimals;
  const sizes = ['Bytes', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

/**
 * Generate a random ID
 * @param {number} length ID length
 * @returns {string} Random ID
 */
export function generateRandomId(length = 8) {
  const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
  let result = '';
  for (let i = 0; i < length; i++) {
    result += chars.charAt(Math.floor(Math.random() * chars.length));
  }
  return result;
}

/**
 * Validate required fields
 * @param {Object} obj Object to validate
 * @param {Array} requiredFields Array of required field names
 * @returns {Object} Validation result { isValid: boolean, missingFields: Array }
 */
export function validateRequiredFields(obj, requiredFields) {
  const missingFields = requiredFields.filter(field =>
    !obj.hasOwnProperty(field) || obj[field] === '' || obj[field] === null || obj[field] === undefined
  );

  return {
    isValid: missingFields.length === 0,
    missingFields
  };
}

/**
 * Detect whether running inside the Tauri environment
 * @returns {boolean} Whether running in Tauri
 */
export function isTauriEnv() {
  return typeof window !== 'undefined' && !!(window.isTauri || window.__TAURI__);
}

/**
 * Open an external link (supports Tauri and browser environments)
 * @param {string} url URL to open
 * @returns {Promise<void>}
 */
export async function openExternalUrl(url) {
  if (!isValidUrl(url)) {
    throw new Error('Invalid URL');
  }

  if (isTauriEnv()) {
    try {
      await window.__TAURI__.shell.open(url);
    } catch (error) {
      console.error('Failed to open URL with Tauri shell:', error);
      // Fall back to window.open
      window.open(url, '_blank');
    }
  } else {
    // Non-Tauri environment, use window.open
    window.open(url, '_blank');
  }
}