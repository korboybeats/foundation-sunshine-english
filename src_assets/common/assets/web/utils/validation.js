/**
 * Form validation utility module
 * Provides validation rules and methods for application forms
 */

/**
 * Validation rules object
 */
export const validationRules = {
  appName: {
    required: true,
    minLength: 1,
    maxLength: 100,
    pattern: /^[^<>:"\\|?*\x00-\x1F]+$/,
    message: 'Application name cannot be empty and cannot contain special characters',
  },
  command: {
    required: false,
    minLength: 0,
    maxLength: 1000,
    message: 'Command is malformed; please enter a valid command',
  },
  workingDir: {
    required: false,
    maxLength: 500,
    message: 'Working directory path is too long',
  },
  outputName: {
    required: false,
    maxLength: 100,
    pattern: /^[a-zA-Z0-9_\-\.]*$/,
    message: 'Output name may only contain letters, digits, underscores, hyphens, and dots',
  },
  timeout: {
    required: false,
    min: 0,
    max: 3600,
    message: 'Timeout must be between 0 and 3600 seconds',
  },
  imagePath: {
    required: false,
    maxLength: 500,
    allowedTypes: ['png', 'jpg', 'jpeg', 'gif', 'bmp', 'webp'],
    message: 'Image path is invalid or the format is not supported',
  },
}

/**
 * Validate a single field
 * @param {string} fieldName Field name
 * @param {any} value Field value
 * @param {Object} customRules Custom validation rules
 * @returns {Object} Validation result {isValid: boolean, message: string}
 */
export function validateField(fieldName, value, customRules = {}) {
  const rules = { ...validationRules[fieldName], ...customRules }

  if (!rules) {
    return { isValid: true, message: '' }
  }

  const strValue = value?.toString().trim() ?? ''
  const isEmpty = strValue === ''

  // Required validation
  if (rules.required && isEmpty) {
    return { isValid: false, message: rules.message || 'This field is required' }
  }

  // If the field is empty and not required, skip the remaining validations
  if (isEmpty) {
    return { isValid: true, message: '' }
  }

  // Length validation
  if (rules.minLength && strValue.length < rules.minLength) {
    return { isValid: false, message: `At least ${rules.minLength} character(s) required` }
  }

  if (rules.maxLength && strValue.length > rules.maxLength) {
    return { isValid: false, message: `At most ${rules.maxLength} character(s) allowed` }
  }

  // Numeric validation
  if (rules.min !== undefined || rules.max !== undefined) {
    const numValue = Number(value)
    if (isNaN(numValue)) {
      return { isValid: false, message: 'Please enter a valid number' }
    }
    if (rules.min !== undefined && numValue < rules.min) {
      return { isValid: false, message: `Minimum value is ${rules.min}` }
    }
    if (rules.max !== undefined && numValue > rules.max) {
      return { isValid: false, message: `Maximum value is ${rules.max}` }
    }
  }

  // Regex validation
  if (rules.pattern && !rules.pattern.test(strValue)) {
    return { isValid: false, message: rules.message || 'Invalid format' }
  }

  // File type validation
  if (rules.allowedTypes && fieldName === 'imagePath' && strValue !== 'desktop') {
    const lastDotIndex = strValue.lastIndexOf('.')
    if (lastDotIndex > 0) {
      const extension = strValue
        .slice(lastDotIndex + 1)
        .split(/[?#]/)[0]
        .toLowerCase()
      if (extension && !rules.allowedTypes.includes(extension)) {
        return {
          isValid: false,
          message: `Only the following formats are supported: ${rules.allowedTypes.join(', ')}`,
        }
      }
    }
  }

  return { isValid: true, message: '' }
}

// Field mapping configuration
const FIELD_MAPPINGS = [
  { key: 'name', rule: 'appName', label: 'Application name' },
  { key: 'cmd', rule: 'command', label: 'Command' },
  { key: 'working-dir', rule: 'workingDir', label: 'Working directory' },
  { key: 'output', rule: 'outputName', label: 'Output name' },
  { key: 'exit-timeout', rule: 'timeout', label: 'Timeout' },
  { key: 'image-path', rule: 'imagePath', label: 'Image path' },
]

/**
 * Validate the application form
 * @param {Object} formData Form data
 * @returns {Object} Validation result
 */
export function validateAppForm(formData) {
  const results = {}
  const errors = []

  // Validate base fields
  for (const { key, rule, label } of FIELD_MAPPINGS) {
    const result = validateField(rule, formData[key])
    results[key] = result
    if (!result.isValid) {
      errors.push(`${label}: ${result.message}`)
    }
  }

  // Validate prep commands
  formData['prep-cmd']?.forEach((cmd, index) => {
    if (!cmd.do?.trim() && !cmd.undo?.trim()) {
      errors.push(`Prep command ${index + 1}: at least one of "run on launch" or "run on exit" must be filled in`)
    }
  })

  // Validate menu commands
  formData['menu-cmd']?.forEach((cmd, index) => {
    if (!cmd.name?.trim()) {
      errors.push(`Menu command ${index + 1}: display name cannot be empty`)
    }
    if (!cmd.cmd?.trim()) {
      errors.push(`Menu command ${index + 1}: command cannot be empty`)
    }
  })

  // Validate detached commands
  formData.detached?.forEach((cmd, index) => {
    if (cmd && !cmd.trim()) {
      errors.push(`Detached command ${index + 1}: command cannot be empty`)
    }
  })

  return {
    isValid: errors.length === 0,
    errors,
    fieldResults: results,
  }
}

/**
 * Validate a file
 * @param {File} file File object
 * @param {Object} options Validation options
 * @returns {Object} Validation result
 */
export function validateFile(file, options = {}) {
  const {
    allowedTypes = ['image/png', 'image/jpg', 'image/jpeg', 'image/gif', 'image/bmp', 'image/webp'],
    maxSize = 10 * 1024 * 1024,
    minSize = 0,
  } = options

  if (!file) {
    return { isValid: false, message: 'Please select a file' }
  }

  if (!allowedTypes.includes(file.type)) {
    return {
      isValid: false,
      message: `Unsupported file type. Supported formats: ${allowedTypes.join(', ')}`,
    }
  }

  if (file.size > maxSize) {
    return {
      isValid: false,
      message: `File size cannot exceed ${(maxSize / (1024 * 1024)).toFixed(1)}MB`,
    }
  }

  if (file.size < minSize) {
    return {
      isValid: false,
      message: `File size cannot be smaller than ${(minSize / 1024).toFixed(1)}KB`,
    }
  }

  return { isValid: true, message: '' }
}

/**
 * Real-time validation mixin
 * @param {Object} formData Form data
 * @param {Array} watchFields Fields to watch
 * @returns {Object} Validation state
 */
export function createFormValidator(formData, watchFields = []) {
  const validationStates = Object.fromEntries(watchFields.map((field) => [field, { isValid: true, message: '' }]))

  return {
    validateField(fieldName, value) {
      const result = validateField(fieldName, value)
      validationStates[fieldName] = result
      return result
    },

    validateForm() {
      return validateAppForm(formData)
    },

    getFieldState(fieldName) {
      return validationStates[fieldName] ?? { isValid: true, message: '' }
    },

    getAllStates() {
      return { ...validationStates }
    },

    resetValidation() {
      for (const key of Object.keys(validationStates)) {
        validationStates[key] = { isValid: true, message: '' }
      }
    },
  }
}

/**
 * Create a debounced validator
 * @param {Function} validationFn Validation function
 * @param {number} delay Debounce delay
 * @returns {Function} Debounced validation function
 */
export function createDebouncedValidator(validationFn, delay = 300) {
  let timeoutId

  return function (...args) {
    clearTimeout(timeoutId)
    return new Promise((resolve) => {
      timeoutId = setTimeout(() => resolve(validationFn(...args)), delay)
    })
  }
}

export default {
  validationRules,
  validateField,
  validateAppForm,
  validateFile,
  createFormValidator,
  createDebouncedValidator,
}
