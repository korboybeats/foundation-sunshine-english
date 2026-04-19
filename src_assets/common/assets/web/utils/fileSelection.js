/**
 * File selection utility module
 * Provides cross-platform file and directory selection
 */

const FILE_FILTERS = [
  { name: 'Executables', extensions: ['exe', 'app', 'sh', 'bat', 'cmd'] },
  { name: 'All files', extensions: ['*'] },
]

const PLACEHOLDERS = {
  windows: { cmd: 'C:\\Program Files\\App\\app.exe', 'working-dir': 'C:\\Program Files\\App' },
  default: { cmd: '/usr/bin/app', 'working-dir': '/usr/bin' },
}

/**
 * File selector class
 */
export class FileSelector {
  constructor(options = {}) {
    this.platform = options.platform || 'linux'
    this.onSuccess = options.onSuccess || (() => {})
    this.onError = options.onError || (() => {})
    this.onInfo = options.onInfo || (() => {})
    this.currentField = null
    this.selectionType = null
  }

  /**
   * Generic select method
   */
  async select(fieldName, input, callback, isDirectory = false) {
    this.currentField = fieldName
    this.selectionType = isDirectory ? 'directory' : 'file'

    if (this.isTauriEnvironment()) {
      return isDirectory ? this.selectDirectoryTauri(fieldName, callback) : this.selectFileTauri(fieldName, callback)
    }

    if (this.isElectronEnvironment()) {
      return isDirectory
        ? this.selectDirectoryElectron(fieldName, callback)
        : this.selectFileElectron(fieldName, callback)
    }

    return isDirectory ? this.selectDirectoryBrowser(input, callback) : this.selectFileBrowser(input, callback)
  }

  /**
   * Select a file
   */
  async selectFile(fieldName, fileInput, callback) {
    return this.select(fieldName, fileInput, callback, false)
  }

  /**
   * Select a directory
   */
  async selectDirectory(fieldName, dirInput, callback) {
    return this.select(fieldName, dirInput, callback, true)
  }

  /**
   * Select a file/directory in the browser environment
   */
  selectBrowser(input, callback, isDirectory) {
    if (!input) {
      this.onError(isDirectory ? 'Directory input element does not exist' : 'File input element does not exist')
      return
    }

    input.value = ''
    input.click()

    const handleSelected = (event) => {
      const files = event.target.files
      const hasSelection = isDirectory ? files.length > 0 : files[0]

      if (hasSelection && this.currentField) {
        try {
          const path = isDirectory ? this.processDirectoryPath(files[0]) : this.processFilePath(files[0])

          callback?.(this.currentField, path)
          this.onSuccess(`${isDirectory ? 'Directory' : 'File'} selected successfully: ${path}`)

          if (!this.isElectronEnvironment()) {
            this.onInfo('In the browser, the full path is unavailable. Please review and adjust the path manually.')
          }
        } catch (error) {
          console.error(`${isDirectory ? 'Directory' : 'File'} selection failed:`, error)
          this.onError(`${isDirectory ? 'Directory' : 'File'} selection failed. Please try again.`)
        }
      }

      this.resetState()
      input.removeEventListener('change', handleSelected)
    }

    input.addEventListener('change', handleSelected)
  }

  selectFileBrowser(fileInput, callback) {
    return this.selectBrowser(fileInput, callback, false)
  }

  selectDirectoryBrowser(dirInput, callback) {
    return this.selectBrowser(dirInput, callback, true)
  }

  /**
   * Detect Tauri environment
   */
  isTauriEnvironment() {
    const tauri = typeof window !== 'undefined' ? window.__TAURI__ : null
    return !!(tauri?.dialog?.open || tauri?.core?.invoke)
  }

  /**
   * Detect Electron environment
   */
  isElectronEnvironment() {
    return typeof window !== 'undefined' && window.process?.type === 'renderer'
  }

  /**
   * Select in the Tauri environment
   */
  async selectTauri(fieldName, callback, isDirectory) {
    const tauri = window.__TAURI__
    if (!tauri?.dialog?.open) {
      this.onError('Tauri dialog API is not available')
      this.resetState()
      return null
    }

    try {
      const options = isDirectory
        ? { title: 'Select directory', multiple: false, directory: true }
        : { title: 'Select file', filters: FILE_FILTERS, multiple: false, directory: false }

      const selected = await tauri.dialog.open(options)

      if (selected) {
        callback?.(fieldName, selected)
        this.onSuccess(`${isDirectory ? 'Directory' : 'File'} selected successfully: ${selected}`)
        this.resetState()
        return selected
      }
    } catch (error) {
      console.error(`Tauri ${isDirectory ? 'directory' : 'file'} selection failed:`, error)
      this.onError(`${isDirectory ? 'Directory' : 'File'} selection failed. Please enter the path manually.`)
    }

    this.resetState()
    return null
  }

  async selectFileTauri(fieldName, callback) {
    return this.selectTauri(fieldName, callback, false)
  }

  async selectDirectoryTauri(fieldName, callback) {
    return this.selectTauri(fieldName, callback, true)
  }

  /**
   * Select in the Electron environment
   */
  async selectElectron(fieldName, callback, isDirectory) {
    try {
      const { dialog } = window.require('electron').remote
      const options = isDirectory
        ? { properties: ['openDirectory'] }
        : { properties: ['openFile'], filters: FILE_FILTERS }

      const result = await dialog.showOpenDialog(options)

      if (!result.canceled && result.filePaths.length > 0) {
        const path = result.filePaths[0]
        callback?.(fieldName, path)
        this.onSuccess(`${isDirectory ? 'Directory' : 'File'} selected successfully: ${path}`)
        return path
      }
    } catch (error) {
      console.error(`${isDirectory ? 'Directory' : 'File'} selection failed:`, error)
      this.onError(`${isDirectory ? 'Directory' : 'File'} selection failed. Please enter the path manually.`)
    }

    this.resetState()
    return null
  }

  async selectFileElectron(fieldName, callback) {
    return this.selectElectron(fieldName, callback, false)
  }

  async selectDirectoryElectron(fieldName, callback) {
    return this.selectElectron(fieldName, callback, true)
  }

  /**
   * Process file path
   */
  processFilePath(file) {
    return file.webkitRelativePath || file.name
  }

  /**
   * Process directory path
   */
  processDirectoryPath(firstFile) {
    if (!firstFile.webkitRelativePath) return ''
    const parts = firstFile.webkitRelativePath.split('/')
    return parts.slice(0, -1).join('/')
  }

  /**
   * Detect development environment
   */
  isDevelopmentEnvironment() {
    if (typeof process !== 'undefined' && process.env?.NODE_ENV === 'development') {
      return true
    }
    if (typeof window !== 'undefined') {
      const { hostname } = window.location
      return hostname === 'localhost' || hostname === '127.0.0.1'
    }
    return false
  }

  /**
   * Reset state
   */
  resetState() {
    this.currentField = null
    this.selectionType = null
  }

  /**
   * Check file selection support
   */
  checkFileSelectionSupport() {
    if (typeof window === 'undefined') return false
    return !!(window.File && window.FileReader && window.FileList && window.Blob)
  }

  /**
   * Check directory selection support
   */
  checkDirectorySelectionSupport(dirInput) {
    return !!(dirInput && 'webkitdirectory' in dirInput)
  }

  /**
   * Get placeholder text for a field
   */
  getPlaceholderText(fieldName) {
    const platformPlaceholders = this.platform === 'windows' ? PLACEHOLDERS.windows : PLACEHOLDERS.default
    return platformPlaceholders[fieldName] || ''
  }

  /**
   * Get button title text
   */
  getButtonTitle(type) {
    return type === 'file' ? 'Select file' : type === 'directory' ? 'Select directory' : 'Select'
  }

  /**
   * Clean up file inputs
   */
  cleanupFileInputs(fileInput, dirInput) {
    if (fileInput) fileInput.value = ''
    if (dirInput) dirInput.value = ''
  }
}

/**
 * Factory function for creating FileSelector instances
 */
export function createFileSelector(options = {}) {
  return new FileSelector(options)
}

/**
 * Convenience file selection function
 */
export async function selectFile(options = {}) {
  const selector = createFileSelector(options)
  return selector.selectFile(options.fieldName, options.fileInput, options.callback)
}

/**
 * Convenience directory selection function
 */
export async function selectDirectory(options = {}) {
  const selector = createFileSelector(options)
  return selector.selectDirectory(options.fieldName, options.dirInput, options.callback)
}

/**
 * Check environment support
 */
export function checkEnvironmentSupport() {
  const selector = createFileSelector()
  return {
    fileSelection: selector.checkFileSelectionSupport(),
    directorySelection: selector.checkDirectorySelectionSupport(),
    isTauri: selector.isTauriEnvironment(),
    isElectron: selector.isElectronEnvironment(),
    isDevelopment: selector.isDevelopmentEnvironment(),
  }
}

export default FileSelector
