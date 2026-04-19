import { APP_CONSTANTS } from './constants.js';

/**
 * Unified error handler class
 */
export class ErrorHandler {
  /**
   * Handle network errors
   * @param {Error} error Error object
   * @param {string} context Error context
   * @returns {string} User-friendly error message
   */
  static handleNetworkError(error, context = 'Operation') {
    console.error(`${context} failed:`, error);

    if (error.name === 'TypeError' && error.message.includes('Failed to fetch')) {
      return `Network connection failed. Please check your network connection and try again.`;
    }

    if (error.message.includes('404')) {
      return `${context} failed: requested resource does not exist`;
    }

    if (error.message.includes('500')) {
      return `${context} failed: internal server error`;
    }

    if (error.message.includes('403')) {
      return `${context} failed: insufficient permissions`;
    }

    return `${context} failed: ${error.message || 'Unknown error'}`;
  }

  /**
   * Handle validation errors
   * @param {Array} errors Error array
   * @returns {string} Formatted error message
   */
  static handleValidationErrors(errors) {
    if (!Array.isArray(errors) || errors.length === 0) {
      return 'Validation failed';
    }

    return errors.join('; ');
  }

  /**
   * Handle application operation errors
   * @param {Error} error Error object
   * @param {string} operation Operation type
   * @param {string} appName Application name
   * @returns {string} Formatted error message
   */
  static handleAppError(error, operation, appName = '') {
    const appContext = appName ? `"${appName}"` : '';

    switch(operation) {
      case 'save':
        return this.handleNetworkError(error, `Save app ${appContext}`);
      case 'delete':
        return this.handleNetworkError(error, `Delete app ${appContext}`);
      case 'load':
        return this.handleNetworkError(error, `Load app ${appContext}`);
      default:
        return this.handleNetworkError(error, `Operate on app ${appContext}`);
    }
  }

  /**
   * Show an error dialog
   * @param {string} message Error message
   * @param {string} title Title
   */
  static showErrorDialog(message, title = 'Error') {
    // For more complex error dialogs, implement them here
    // For now, use a simple alert
    alert(`${title}\n\n${message}`);
  }

  /**
   * Show a confirmation dialog
   * @param {string} message Confirmation message
   * @param {string} title Title
   * @returns {boolean} Whether the user confirmed
   */
  static showConfirmDialog(message, title = 'Confirm') {
    return confirm(`${title}\n\n${message}`);
  }

  /**
   * Log an error
   * @param {Error} error Error object
   * @param {string} context Error context
   * @param {Object} metadata Additional metadata
   */
  static logError(error, context = '', metadata = {}) {
    const errorInfo = {
      message: error.message,
      stack: error.stack,
      context,
      timestamp: new Date().toISOString(),
      ...metadata
    };

    console.error('Application error:', errorInfo);

    // If sending to a logging service is needed, implement it here
    // this.sendToLogService(errorInfo);
  }

  /**
   * Handle errors from asynchronous operations
   * @param {Promise} promise Promise object
   * @param {string} context Error context
   * @returns {Promise} Wrapped promise
   */
  static async handleAsyncError(promise, context = '') {
    try {
      return await promise;
    } catch (error) {
      this.logError(error, context);
      throw error;
    }
  }
}
