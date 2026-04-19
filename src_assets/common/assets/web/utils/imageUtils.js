/**
 * Image utility functions
 * Used to normalize application image URL handling
 */
/**
 * Get image preview URL
 * @param {string} imagePath Image path
 * @returns {string} Preview URL
 */
export function getImagePreviewUrl(imagePath = 'box.png') {
  if (imagePath === 'desktop') {
    return '/boxart/desktop.png'
  }
  // If the path contains no separator, it is a boxart resource ID
  if (!/[/\\]/.test(imagePath)) {
    return `/boxart/${encodeURIComponent(imagePath)}`
  }

  return isLocalImagePath(imagePath) ? `file://${imagePath}` : imagePath
}

/**
 * Check whether the image path is a local file path
 * @param {string} imagePath Image path
 * @returns {boolean} Whether it is a local file path
 */
export function isLocalImagePath(imagePath) {
  if (!imagePath) {
    return false
  }

  // If it is a network URL or blob/data URL, it is not a local path
  if (
    imagePath.startsWith('http://') ||
    imagePath.startsWith('https://') ||
    imagePath.startsWith('blob:') ||
    imagePath.startsWith('data:')
  ) {
    return false
  }

  return true
}
