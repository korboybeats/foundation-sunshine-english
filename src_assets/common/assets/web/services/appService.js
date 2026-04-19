import { API_ENDPOINTS } from '../utils/constants.js';
import { formatError } from '../utils/helpers.js';

/**
 * Application service class
 */
export class AppService {
  /**
   * Get the application list
   * @returns {Promise<Array>} Application list
   */
  static async getApps() {
    try {
      const response = await fetch(API_ENDPOINTS.APPS);
      if (!response.ok) {
        throw new Error(`Failed to load app list: ${response.status}`);
      }
      const data = await response.json();
      return data.apps || [];
    } catch (error) {
      console.error('Failed to load app list:', error);
      throw new Error(formatError(error));
    }
  }

  /**
   * Save apps
   * @param {Array} apps Application list
   * @param {Object} editApp App being edited (optional)
   * @returns {Promise<boolean>} Whether the save succeeded
   */
  static async saveApps(apps, editApp = null) {
    try {
      const response = await fetch(API_ENDPOINTS.APPS, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          apps,
          editApp
        })
      });

      if (!response.ok) {
        throw new Error(`Failed to save app: ${response.status}`);
      }

      return true;
    } catch (error) {
      console.error('Failed to save app:', error);
      throw new Error(formatError(error));
    }
  }

  /**
   * Delete an app
   * @param {number} index App index
   * @returns {Promise<boolean>} Whether the deletion succeeded
   */
  static async deleteApp(index) {
    try {
      const response = await fetch(API_ENDPOINTS.APP_DELETE(index), {
        method: 'DELETE'
      });

      if (!response.ok) {
        throw new Error(`Failed to delete app: ${response.status}`);
      }

      return true;
    } catch (error) {
      console.error('Failed to delete app:', error);
      throw new Error(formatError(error));
    }
  }

  /**
   * Get platform info
   * @returns {Promise<string>} Platform info
   */
  static async getPlatform() {
    try {
      const response = await fetch(API_ENDPOINTS.CONFIG);
      if (!response.ok) {
        throw new Error(`Failed to fetch platform info: ${response.status}`);
      }
      const data = await response.json();
      return data.platform || 'windows';
    } catch (error) {
      console.error('Failed to fetch platform info:', error);
      // Default to windows
      return 'windows';
    }
  }

  /**
   * Search apps
   * @param {Array} apps Application list
   * @param {string} query Search keyword
   * @returns {Array} Search result
   */
  static searchApps(apps, query) {
    if (!query || !query.trim()) {
      return [...apps];
    }

    const searchTerm = query.toLowerCase().trim();
    return apps.filter(app =>
      app.name.toLowerCase().includes(searchTerm) ||
      (app.cmd && app.cmd.toLowerCase().includes(searchTerm))
    );
  }

  /**
   * Validate app data
   * @param {Object} app App object
   * @returns {Object} Validation result
   */
  static validateApp(app) {
    const errors = [];

    if (!app.name || !app.name.trim()) {
      errors.push('Application name cannot be empty');
    }

    if (!app.cmd || !app.cmd.trim()) {
      errors.push('Application command cannot be empty');
    }

    // Validate exit timeout
    if (app['exit-timeout'] !== undefined &&
        (isNaN(app['exit-timeout']) || app['exit-timeout'] < 0)) {
      errors.push('Exit timeout must be a non-negative number');
    }

    return {
      isValid: errors.length === 0,
      errors
    };
  }

  /**
   * Format app data
   * @param {Object} app Raw app data
   * @returns {Object} Formatted app data
   */
  static formatAppData(app) {
    // Filter out prep-cmd entries where both do and undo are empty or contain only whitespace
    const filteredPrepCmd = Array.isArray(app['prep-cmd'])
      ? app['prep-cmd'].filter(cmd => {
          const hasDo = cmd.do && cmd.do.trim() !== '';
          const hasUndo = cmd.undo && cmd.undo.trim() !== '';
          // Keep if at least one is non-empty
          return hasDo || hasUndo;
        })
      : [];

    return {
      name: app.name?.trim() || '',
      output: app.output?.trim() || '',
      cmd: app.cmd?.trim() || '',
      'exclude-global-prep-cmd': Boolean(app['exclude-global-prep-cmd']),
      elevated: Boolean(app.elevated),
      'auto-detach': Boolean(app['auto-detach']),
      'wait-all': Boolean(app['wait-all']),
      'exit-timeout': parseInt(app['exit-timeout']) || 5,
      'prep-cmd': filteredPrepCmd,
      'menu-cmd': Array.isArray(app['menu-cmd']) ? app['menu-cmd'] : [],
      detached: Array.isArray(app.detached) ? app.detached : [],
      'image-path': app['image-path']?.trim() || '',
      'working-dir': app['working-dir']?.trim() || ''
    };
  }
}
