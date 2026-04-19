import { onMounted } from 'vue'
import { loadAutoTheme, showActiveTheme, getPreferredTheme } from '../utils/theme.js'

export function useTheme() {
  onMounted(() => {
    loadAutoTheme()
    showActiveTheme(getPreferredTheme(), false)
  })

  return {
    // Additional theme-related features can be exposed here
  }
}
